#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <sys/inotify.h>
#include <sys/signal.h>
#include "../include/tcp-socket.hpp"
#include "../include/file-io.hpp"
#include "../include/watcher.hpp"
#include "../include/message.hpp"
#include "../include/snapshot-manager.hpp"
#include "../include/messenger.hpp"
#include "../include/file-change-handler.hpp"
#include "../include/server-message-handler.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

std::function<void(int)> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler(sig);
}

int main()
{
    // configuring snap manager and watcher over the working dir
    SnapshotManager snap_manager(DATA_DIR, SNAP_FILE);
    Watcher watcher(DATA_DIR);

    // create connection to server
    TcpConnection client(SERVER_IP, std::to_string(PORT));

    // configuring messenger to send/receive messages
    Messenger messenger(client);

    // configuring change handler to sync changes
    FileChangeHandler file_change_handler(messenger, DATA_DIR);

    // get server's snap and current snap
    auto &&[client_snap_version, curr_snap] = snap_manager.scan_directory();
    auto &&[server_snap_version, server_snap] = snap_manager.load_snapshot();

    const auto fetch_server_snap = [&]()
    {
        Message msg;

        msg.type = MessageType::REQ_SNAP;
        msg.payload = {};

        messenger.send_json_message(msg);

        // receive snap of data from server
        Message &&server_message = messenger.receive_json_message();

        if (server_message.type != MessageType::DATA_SNAP)
            throw std::runtime_error("invalid type of message");

        if (auto payload_ptr = std::get_if<DataSnapshotPayload>(&(server_message.payload)))
        {
            for (auto &file : payload_ptr->files)
                server_snap[file.filename] = std::move(file);
        }
        else
            throw std::runtime_error("invalid data received");
    };

    // for checking if initial changes
    const bool is_server_snap_present = !server_snap.empty();
    bool is_snap_version_same = false;

    // when snap available then request server to send snap version
    if (is_server_snap_present)
    {
        Message msg;
        msg.type = MessageType::REQ_SNAP_VERSION;
        msg.payload = {};

        messenger.send_json_message(msg);

        const Message &server_message = messenger.receive_json_message();
        if (server_message.type != MessageType::SNAP_VERSION)
            throw std::runtime_error("invalid type received");

        if (auto payload = std::get_if<SnapVersionPayload>(&(server_message.payload)))
        {
            is_snap_version_same = payload->snap_version == server_snap_version;
        }
    }

    // when snap version not same then fetch snap from server
    if (!is_snap_version_same)
        fetch_server_snap();

    // compare both snapshots and find changes
    DirChanges &&dir_changes = snap_manager.compare_snapshots(curr_snap, server_snap);

    if (dir_changes.created_files.empty() && dir_changes.modified_files.empty() && dir_changes.removed_files.empty())
        std::clog << "no initial changes found" << std::endl;

    // sync changes as server snap present
    // server snap is a sign that we have previously synced to server
    else if (is_server_snap_present)
    {
        std::clog << "initial changes found!! syncing..." << std::endl;
        file_change_handler.handle_changes(dir_changes);
    }

    // download data from server as client's data needs to be updatd
    else
    {
        Message msg;
        msg.type = MessageType::REQ_DOWNLOAD_FILES;

        // deleted files are to be created by fetching from server
        // created files are to be deleted
        // modified files are to be modified at client

        RequestDownloadFilesPayload payload;
        payload.files = dir_changes.removed_files;
        msg.payload = std::move(payload);

        messenger.send_json_message(msg);

        ServerMessageHandler server_message_handler(DATA_DIR, client);

        // write chunks to new files
        while (true)
        {
            const Message &server_message = messenger.receive_json_message();

            if (auto payload = std::get_if<SendChunkPayload>(&(server_message.payload)))
                server_message_handler.process_file_chunk(*payload);
            else
                break;
        }

        // now remove all the created files to match server's snap
        for (const auto &created_file : dir_changes.created_files)
            fs::remove(std::string(DATA_DIR) + "/" + created_file.filename);

        // now time to handle file modification
        for (const auto &modified_file : dir_changes.modified_files)
        {
            const auto &added_chunks = modified_file.added;
            const auto &removed_chunks = modified_file.removed;
            const auto &modified_chunks = modified_file.modified;

            const size_t total_modified = modified_chunks.size();
            const size_t total_added = added_chunks.size();
            const size_t total_removed = removed_chunks.size();

            const std::string &filepath = std::string(DATA_DIR) + "/" + modified_file.filename;

            FileIO fileio(filepath);

            for (size_t i = 0, j = 0, k = 0;
                 i < total_added || j < total_removed || k < total_removed;)
            {
                AddRemoveChunkPayload added_chunk;
                AddRemoveChunkPayload removed_chunk;
                ModifiedChunkPayload modified_chunk;

                if (i < added_chunks.size())
                    added_chunk = added_chunks[i];
                if (j < removed_chunks.size())
                    removed_chunk = removed_chunks[j];
                if (k < modified_chunks.size())
                    modified_chunk = modified_chunks[k];

                // check if really using min_offset works
                const size_t min_offset = std::min(added_chunk.offset, std::min(removed_chunk.offset, modified_chunk.offset));

                if (min_offset == added_chunk.offset)
                {
                    size_t chunk_size = added_chunk.chunk_size;
                    server_message_handler.process_add_remove_chunk(added_chunk, true);
                    ++i;
                }
                else if (min_offset == removed_chunk.offset)
                {
                    server_message_handler.process_add_remove_chunk(removed_chunk, false);

                    // fetch the chunk from server and add it to your corresponding file
                    ++j;
                }
                else if (min_offset == modified_chunk.offset)
                {
                    size_t chunk_size = modified_chunk.chunk_size;

                    ++k;
                }
            }
        }
    }

    // after syncing changes now save the server's snap
    snap_manager.save_snapshot(curr_snap);

    signal_handler = [&client](int _)
    {
        client.closeConnection();
        exit(EXIT_SUCCESS);
    };
    signal(SIGINT, signal_handler_wrap);

    // continuously watch for changes to sync
    while (true)
    {
        const auto &events = watcher.poll_events();

        for (const auto &event : events)
            file_change_handler.handle_event(event, curr_snap);
    }

    return 0;
}