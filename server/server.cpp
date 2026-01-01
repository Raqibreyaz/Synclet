#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <atomic>
#include "../include/tcp-socket.hpp"
#include "../include/snapshot-manager.hpp"
#include "../include/messenger.hpp"
#include "../include/message-types.hpp"
#include "../include/message.hpp"
#include "../include/sender-message-handler.hpp"
#include "../include/receiver-message-handler.hpp"

#define PORT 9000
#define DATA_DIR "server/data"
#define SNAP_FILE "snap-file.json"

std::atomic<bool> should_shutdown{false};
void signal_handler(int sig)
{
    should_shutdown = true;
}

// using templates to handle repetitive payload handling
template <typename PayloadType, typename Handler>
void handle_message_with_payload(const Message &msg, Handler &&handler)
{
    auto payload = std::get_if<PayloadType>(&(msg.payload));
    if (payload)
        handler(*payload);
    else
        std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
}

int main(int argc, const char *argv[])
{
    const std::string user_home = std::getenv("HOME");
    const std::unordered_map<std::string, std::string> cli_args = parse_cli_args(argc, argv);

    const std::string peer_snap_filepath = user_home + "/" + SNAP_FILE;
    const std::string data_dir_path = cli_args.contains("datadir") ? cli_args.at("datadir") : DATA_DIR;
    const std::string port = cli_args.contains("port") ? cli_args.at("port") : std::to_string(PORT);

    // create the data dir if not exists
    if (!fs::exists(data_dir_path))
        fs::create_directory(data_dir_path);

    // a snapshot manager
    SnapshotManager snap_manager(data_dir_path, peer_snap_filepath);

    // create snapshot by analysing the whole directory
    auto &&[snap_version, snaps] = snap_manager.scan_directory();

    // prefer error when client disconnects instead of silence
    signal(SIGPIPE, SIG_IGN);

    // handle for ctrl + c
    signal(SIGINT, signal_handler);

    try
    {
        // create a server on wildcard address
        TcpServer server("0.0.0.0", port);
        std::clog << "Server is Listening on Port: " << port << std::endl;

        // continuously listen until a client connects
        while (!should_shutdown)
        {
            TcpConnection client = server.acceptClient();
            Messenger messenger(client);

            ReceiverMessageHandler receiver_message_handler(data_dir_path, messenger);
            SenderMessageHandler sender_message_handler(messenger, data_dir_path);

            try
            {
                // work with the connected client until it disconnects
                while (!should_shutdown)
                {
                    const Message &msg = messenger.receive_json_message();

                    std::clog << "message type is: " << message_type_to_string(msg.type) << std::endl;

                    switch (msg.type)
                    {

                    // create file and receive data
                    case MessageType::FILE_CREATE:
                        handle_message_with_payload<FileCreateRemovePayload>(msg, [&](const auto &p)
                                                                             { receiver_message_handler.process_create_file(p, snaps); });
                        break;

                    // remove the given file
                    case MessageType::FILE_REMOVE:
                        handle_message_with_payload<FileCreateRemovePayload>(msg, [&](const auto &p)
                                                                             { receiver_message_handler.process_delete_file(p, snaps); });
                        break;

                    // create the given files and append provided data to them
                    case MessageType::FILES_CREATE:
                        handle_message_with_payload<FilesCreatedPayload>(msg, [&](const auto &p)
                                                                         { receiver_message_handler.process_create_file(p, snaps); });
                        break;

                    // remove given files
                    case MessageType::FILES_REMOVE:
                        handle_message_with_payload<FilesRemovedPayload>(msg, [&](const auto &p)
                                                                         { receiver_message_handler.process_delete_file(p, snaps); });
                        break;

                    // rename the given file
                    case MessageType::FILE_MOVED:
                        handle_message_with_payload<FileMovedPayload>(msg, [&](const auto &p)
                                                                      { receiver_message_handler.process_file_moved(p, snaps); });
                        break;

                    case MessageType::DIR_CREATE:
                        handle_message_with_payload<DirCreateRemovePayload>(msg, [&](const auto &p)
                                                                            { receiver_message_handler.process_create_dir(p); });
                        break;

                    case MessageType::DIR_REMOVE:
                        handle_message_with_payload<DirCreateRemovePayload>(msg, [&](const auto &p)
                                                                            { receiver_message_handler.process_delete_dir(p, snaps); });
                        break;

                    case MessageType::DIRS_CREATE:
                        handle_message_with_payload<DirsCreatedRemovedPayload>(msg, [&](const auto &p)
                                                                               { receiver_message_handler.process_create_dir(p); });
                        break;

                    case MessageType::DIRS_REMOVE:
                        handle_message_with_payload<DirsCreatedRemovedPayload>(msg, [&](const auto &p)
                                                                               { receiver_message_handler.process_delete_dir(p, snaps); });
                        break;

                    case MessageType::DIR_MOVED:
                        handle_message_with_payload<DirMovedPayload>(msg, [&](const auto &p)
                                                                     { receiver_message_handler.process_dir_moved(p, snaps); });
                        break;

                    // save the modified chunk in corresponding file
                    case MessageType::MODIFIED_CHUNK:
                        handle_message_with_payload<ModifiedChunkPayload>(msg, [&](const auto &p)
                                                                          { receiver_message_handler.process_modified_chunk(p, snaps); });
                        break;

                    case MessageType::SEND_FILE:
                        handle_message_with_payload<SendFilePayload>(msg, [&](const auto &p)
                                                                     { receiver_message_handler.process_file(p, snaps); });
                        break;

                    // append the given chunk in corresponding file
                    case MessageType::SEND_CHUNK:
                        handle_message_with_payload<SendChunkPayload>(msg, [&](const auto &p)
                                                                      { receiver_message_handler.process_file_chunk(p, snaps); });
                        break;

                    // send the snapshot version to peer as per request
                    case MessageType::REQ_SNAP_VERSION:
                        sender_message_handler.handle_request_snap_version(snap_version);
                        break;

                    // send the whole snapshot to peer as per request
                    case MessageType::REQ_SNAP:
                        sender_message_handler.handle_request_snap(snaps);
                        break;

                    case MessageType::REQ_DIR_LIST:
                        sender_message_handler.handle_request_dir_list();
                        break;

                    // send the requested chunk to peer
                    case MessageType::REQ_CHUNK:
                        handle_message_with_payload<RequestChunkPayload>(msg, [&](const auto &p)
                                                                         { sender_message_handler.handle_request_chunk(p); });
                        break;

                    // send the requested files to peer
                    case MessageType::REQ_DOWNLOAD_FILES:
                        handle_message_with_payload<RequestDownloadFilesPayload>(msg, [&](const auto &p)
                                                                                 { sender_message_handler.handle_request_download_files(p, snaps); });
                        break;

                    default:
                        std::cerr << "unknown message type found" << std::endl;
                        break;
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }

            client.closeConnection();
            std::clog << "Client session ended, waiting for next connection...\n\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    std::clog << "Server shutting down...\n";
    return 0;
}