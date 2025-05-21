#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <sys/inotify.h>
#include "../include/utils.hpp"
#include "../include/tcp-socket.hpp"
#include "../include/file-io.hpp"
#include "../include/watcher.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

int main()
{
    // initially server and client either will have snapshot to get an understanding of what to update on data

    // get prev and current snapshots
    auto curr_snap = scan_directory(DATA_DIR);
    auto prev_snap = curr_snap;

    DirChanges &&dir_changes = compare_snapshots(curr_snap, prev_snap);

    // create connection to server
    TcpConnection client(SERVER_IP, std::to_string(PORT));

    Watcher watcher(DATA_DIR);

    json j = nullptr;
    Message msg;

    auto message_sender = [&](const std::function<void(void)> &callback = nullptr)
    {
        if (!j || j.empty())
            return;

        to_json(j, msg);

        const std::string message = j.dump();

        // sending the len of json message
        std::string json_len_string = convert_to_binary_string(message.size());
        client.sendAll(json_len_string);

        std::clog << "sending message to server: " << message << std::endl;
        client.sendAll(message);

        if (!callback)
            callback();
    };

    auto data_sender = [&](FileIO &fileio, size_t offset, size_t chunk_size)
    {
        std::string chunk_string = fileio.read_file_from_offset(offset, chunk_size);

        // send the message raw data
        client.sendAll(chunk_string);

        // now send EOF to stop waiting
        client.shutdownWrite();
    };

    // contuously watch for changes to sync
    while (true)
    {
        auto events = watcher.poll_events();

        for (auto &event : events)
        {
            const EventType event_type = event.event_type;
            const std::string filepath = event.filepath.string();
            const std::string filename = extract_filename_from_path(filepath);

            // when file is created or deleted then inform server
            if (event_type == EventType::CREATED || event_type == EventType::DELETED)
            {
                msg.type = event_type == EventType::CREATED ? Type::FILE_CREATE : Type::FILE_REMOVE;
                msg.payload = FileCreateRemovePayload{
                    .filename = filename};

                message_sender();
            }

            // handle file rename case, inform server
            else if (event_type == EventType::RENAMED)
            {
                // skip the event if it is incomplete
                if (!(event.new_filepath.has_value()) && !(event.old_filepath.has_value()))
                    continue;

                // create the message
                msg.type = Type::FILE_RENAME;
                msg.payload = FileRenamePayload{
                    .old_filename = event.new_filepath.value().string(),
                    .new_filename = event.old_filepath.value().string()};

                message_sender();
            }

            // handle file modify case
            else
            {
                FileSnapshot curr_file_snap = createSnapshot(filepath,
                                                             fs::file_size(filepath),
                                                             to_unix_timestamp(fs::last_write_time(filepath)));

                FileSnapshot prev_file_snap = prev_snap[filename];

                FileModification file_modification = get_file_modification(curr_file_snap, prev_file_snap);

                // open the file for IO
                FileIO fileio(filepath);

                // modified chunks
                for (int &chunk_no : file_modification.modified)
                {
                    msg.type = Type::SEND_CHUNK;
                    msg.payload = SendChunkPayload{
                        .filename = filename,
                        .chunk_no = chunk_no,
                        .chunk_size = curr_file_snap.chunks[chunk_no].chunk_size,
                        .offset = curr_file_snap.chunks[chunk_no].offset,
                        .is_removed = false,
                        .is_last_chunk = curr_file_snap.chunks.size() == static_cast<size_t>(chunk_no - 1),
                    };

                    // send the message
                    message_sender();

                    // now send the actual data
                    data_sender(fileio,
                                curr_file_snap.chunks[chunk_no].offset,
                                curr_file_snap.chunks[chunk_no].chunk_size);
                }

                for (int &chunk_no : file_modification.removed)
                {
                    msg.type = Type::FILE_REMOVE;
                    msg.payload = SendChunkPayload{
                        .filename = filename,
                        .chunk_no = chunk_no,
                        .chunk_size = prev_file_snap.chunks[chunk_no].chunk_size,
                        .offset = prev_file_snap.chunks[chunk_no].offset,
                        .is_removed = true,
                        .is_last_chunk = false,
                    };

                    // send the message
                    message_sender();

                    // now send the actual raw data
                    data_sender(fileio,
                                prev_file_snap.chunks[chunk_no].offset,
                                prev_file_snap.chunks[chunk_no].chunk_size);
                }
            }
        }
    }

    client.closeConnection();

    return 0;
}
