#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <ranges>
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

void func()
{
}

int main()
{
    // get prev and current snapshots
    auto curr_snap = scan_directory(DATA_DIR);
    auto prev_snap = curr_snap;
    auto fileRenameTracker = std::unordered_map<int, FileRenamePayload>();

    // create connection to server
    TcpConnection client(SERVER_IP, std::to_string(PORT));

    Watcher watcher(DATA_DIR);

    json j = nullptr;
    Message msg;

    while (true)
    {
        auto events = watcher.poll_events();

        for (auto &event : events)
        {
            const std::string filepath = std::string(DATA_DIR) + "/" + event->name;

            std::clog << "filepath: " << filepath << std::endl;

            // when file is created or deleted then inform server
            if ((event->mask & IN_CREATE) || (event->mask & IN_DELETE))
            {
                msg.type = (event->mask & IN_CREATE) ? Type::FILE_CREATE : Type::FILE_REMOVE;
                msg.payload = FileCreateRemovePayload{.filename = event->name};

                to_json(j, msg);

                const std::string message = j.dump();

                std::clog << "sending message to server: " << message << std::endl;

                client.sendAll(message);
            }

            // handle file rename case, inform server
            else if ((event->mask & IN_MOVED_FROM) || (event->mask & IN_MOVED_TO))
            {
                auto &file_rename = fileRenameTracker[event->cookie];

                auto filename = event->name;

                if (event->mask & IN_MOVED_FROM)
                    file_rename.old_filename = filename;
                else
                    file_rename.new_filename = filename;

                // when both old and new filename got then inform server
                if (!(file_rename.old_filename.empty()) && !(file_rename.new_filename.empty()))
                {
                    msg.type = Type::FILE_RENAME;
                    msg.payload = file_rename;
                    to_json(j, msg);

                    std::string message = j.dump();
                    std::clog << "sending message to server: " << message << std::endl;

                    client.sendAll(j.dump());
                }
            }

            // handle file modify case
            else
            {
                std::clog << "file modified" << std::endl;

                std::string filename = event->name;

                FileSnapshot curr_file_snap = createSnapshot(filepath, fs::file_size(filepath), to_unix_timestamp(fs::last_write_time(filepath)));

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

                    to_json(j, msg);
                    std::string message = j.dump();

                    std::clog << "message length: " << message.size() << std::endl;

                    // send the data size
                    uint32_t json_len = htonl(static_cast<uint32_t>(message.size()));

                    std::clog << "json len: " << json_len << std::endl;

                    std::string json_len_string(reinterpret_cast<const char *>(&json_len), sizeof(json_len));

                    std::clog << "sending json length " << json_len_string << std::endl;

                    client.sendAll(json_len_string);

                    std::clog << "sending message to server: " << message << std::endl;

                    // send the message type and payload
                    client.sendAll(message);

                    std::string chunk_string = fileio.read_file_from_offset(curr_file_snap.chunks[chunk_no].offset, curr_file_snap.chunks[chunk_no].chunk_size);

                    std::clog << "sending chunk of size: " << chunk_string.size() << std::endl;

                    // send the message raw data
                    client.sendAll(chunk_string);

                    // now send EOF to stop waiting
                    client.shutdownWrite();
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

                    to_json(j, msg);
                    std::string message = j.dump();
                    std::clog << "sending message to client: " << message << std::endl;

                    // send the size of the json
                    uint32_t json_len = htonl(message.size());
                    client.sendAll(std::string(reinterpret_cast<char *>(&json_len), sizeof(json_len)));

                    // now send the json
                    client.sendAll(message);

                    // now send the actual raw data
                    client.sendAll(fileio.read_file_from_offset(prev_file_snap.chunks[chunk_no].offset, prev_file_snap.chunks[chunk_no].chunk_size));
                }
            }
        }
    }
    client.closeConnection();

    return 0;
}
