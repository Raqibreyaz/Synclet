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
#include "../include/sender-message-handler.hpp"
#include "../include/receiver-message-handler.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define DATA_DIR "./data"
#define PEER_SNAP_FILE "./peer-snap-file.json"

std::function<void(int)> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler(sig);
}

int main()
{
    try
    {
        // configuring snap manager and watcher over the working dir
        SnapshotManager snap_manager(DATA_DIR, PEER_SNAP_FILE);
        Watcher watcher(DATA_DIR);

        // create connection to server
        TcpConnection client(SERVER_IP, std::to_string(PORT));

        // configuring messenger to send/receive messages
        Messenger messenger(client);

        // configuring sending message handler to sync changes
        SenderMessageHandler sender_message_handler(messenger, DATA_DIR);

        // configuring receiving message handler to request & receive message
        ReceiverMessageHandler receiver_message_handler(DATA_DIR, messenger);

        // get current snap and peer's snap
        auto [curr_snap_version, curr_snap] = snap_manager.scan_directory();
        auto [peer_snap_version, peer_snap] = snap_manager.load_snapshot();

        const bool was_peer_snap_present = !peer_snap.empty();
        std::string fetched_peer_snap_version = receiver_message_handler.process_request_snap_version();
        const bool was_peer_snap_version_same = was_peer_snap_present &&
                                                fetched_peer_snap_version == peer_snap_version;

        // if no snap present or new snap available then fetch snap + update peer_snap and it's snap version
        if (!was_peer_snap_present || !was_peer_snap_version_same)
        {
            receiver_message_handler.process_request_peer_snap(peer_snap);
            peer_snap_version = fetched_peer_snap_version;
        }

        // compare both snapshots and find changes
        DirChanges &&dir_changes = snap_manager.compare_snapshots(curr_snap, peer_snap);

        if (dir_changes.created_files.empty() &&
            dir_changes.modified_files.empty() &&
            dir_changes.removed_files.empty())
            std::clog << "no initial changes found" << std::endl;

        // when files are created then sync them to peer
        if (!dir_changes.created_files.empty())
        {

            std::clog << "syncing new files to peer..." << std::endl;
            sender_message_handler.handle_create_file(dir_changes.created_files);
        }

        // when files are not present
        if (!dir_changes.removed_files.empty())
        {
            // fetch all the deleted files from peer if it's snap wasn't present or new snap available
            // + update the curr_snap
            if (!was_peer_snap_present || !was_peer_snap_version_same)
            {
                std::clog << "downloading new files from peer..." << std::endl;
                receiver_message_handler.process_fetch_files(dir_changes.removed_files, curr_snap);
            }

            // when curr snap was up to date then ask server to delete the files
            else
            {
                std::clog << "requesting peer to delete files" << std::endl;
                sender_message_handler.handle_delete_file(dir_changes.removed_files);
            }
        }

        // when files are modified
        if (!dir_changes.modified_files.empty())
        {
            std::vector<FileModification> to_fetch;
            std::vector<FileModification> to_change;

            for (const auto &modified_file : dir_changes.modified_files)
            {
                // more time_t -> more recent
                // if the current file is more older then fetch the modified chunks from peer
                if (curr_snap[modified_file.filename].mtime < peer_snap[modified_file.filename].mtime)
                    to_fetch.push_back(modified_file);

                // if current file is newer then sync changes to peer
                else
                    to_change.push_back(modified_file);
            }

            // request peer to update these files
            if (!to_change.empty())
            {
                std::clog << "syncing file changes to peer" << std::endl;
                sender_message_handler.handle_modify_file(to_change);
            }

            // request peer to send these updated chunks and save them + update the curr_snap
            if (!to_fetch.empty())
            {
                std::clog << "fetching modified files from peer" << std::endl;
                receiver_message_handler.process_fetch_modified_chunks(to_fetch, curr_snap);
            }
        }

        // now after syncing all changes save the current snap as peer's snap
        if (!dir_changes.created_files.empty() || !dir_changes.modified_files.empty() || !dir_changes.removed_files.empty())
        {
            std::clog << "saving peer snap as cache" << std::endl;
            snap_manager.save_snapshot(curr_snap);
        }

        signal_handler = [&](int _)
        {
            client.closeConnection();
            snap_manager.save_snapshot(curr_snap);
        };
        exit(EXIT_SUCCESS);
        signal(SIGINT, signal_handler_wrap);

        // continuously watch for changes to sync
        std::clog << "waiting for file changes..." << std::endl;
        while (true)
        {
            const auto &events = watcher.poll_events();

            for (const auto &event : events)
                sender_message_handler.handle_event(event, curr_snap);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}