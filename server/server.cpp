// server.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../include/tcp-socket.hpp"
#include "../include/snapshot-manager.hpp"
#include "../include/messenger.hpp"
#include "../include/message-types.hpp"
#include "../include/message.hpp"
#include "../include/sender-message-handler.hpp"
#include "../include/receiver-message-handler.hpp"

#define PORT 9000
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
    SnapshotManager snap_manager(DATA_DIR, SNAP_FILE);

    try
    {
        // fetch the snaps from SNAP_FILE
        auto &&[snap_version, snaps] = snap_manager.scan_directory();

        // create a server on localhost
        TcpServer server("127.0.0.1", std::to_string(PORT));
        TcpConnection client = server.acceptClient();
        Messenger messenger(client);

        signal_handler = [&client](int _)
        {
            client.closeConnection();
            exit(EXIT_SUCCESS);
        };

        signal(SIGINT, signal_handler_wrap);

        ReceiverMessageHandler receiver_message_handler(DATA_DIR, messenger);
        SenderMessageHandler sender_message_handler(messenger, DATA_DIR);

        while (true)
        {
            const Message &msg = messenger.receive_json_message();

            std::clog << "message type is: " << message_type_to_string(msg.type) << std::endl;

            switch (msg.type)
            {

            // create file and receive data
            case MessageType::FILE_CREATE:
            {
                auto payload = std::get_if<FileCreateRemovePayload>(&(msg.payload));
                if (payload)
                    receiver_message_handler.process_create_file(*payload,snaps);
                  
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // remove the given file
            case MessageType::FILE_REMOVE:
            {
                if (auto payload = std::get_if<FileCreateRemovePayload>(&(msg.payload)))
                    receiver_message_handler.process_delete_file(*payload,snaps);
                   
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // create the given files and append provided data to them
            case MessageType::FILES_CREATE:
            {
                if (auto payload = std::get_if<FilesCreatedPayload>(&(msg.payload)))
                    receiver_message_handler.process_create_file(*payload,snaps);

                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);

                break;
            }

            // remove given files
            case MessageType::FILES_REMOVE:
            {
                if (auto payload = std::get_if<FilesRemovedPayload>(&(msg.payload)))
                    receiver_message_handler.process_delete_file(*payload,snaps);
        
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // rename the given file
            case MessageType::FILE_RENAME:
            {
                if (auto payload = std::get_if<FileRenamePayload>(&(msg.payload)))
                    receiver_message_handler.process_file_rename(*payload,snaps);

                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // save the modified chunk in corresponding file
            case MessageType::MODIFIED_CHUNK:
            {
                if (auto payload = std::get_if<ModifiedChunkPayload>(&(msg.payload)))
                    receiver_message_handler.process_modified_chunk(*payload,snaps);
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // append the given chunk in corresponding file
            case MessageType::SEND_CHUNK:
            {
                if (auto payload = std::get_if<SendChunkPayload>(&(msg.payload)))
                    receiver_message_handler.process_file_chunk(*payload,snaps);
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // send the snapshot version to peer as per request
            case MessageType::REQ_SNAP_VERSION:
            {
                sender_message_handler.handle_request_snap_version(snap_version);
                break;
            }

            // send the whole snapshot to peer as per request
            case MessageType::REQ_SNAP:
            {
                sender_message_handler.handle_request_snap(snaps);
                break;
            }

            // send the requested chunk to peer
            case MessageType::REQ_CHUNK:
            {
                if (auto payload = std::get_if<RequestChunkPayload>(&(msg.payload)))
                    sender_message_handler.handle_request_chunk(*payload);
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // send the requested files to peer
            case MessageType::REQ_DOWNLOAD_FILES:
            {
                if (auto payload = std::get_if<RequestDownloadFilesPayload>(&(msg.payload)))
                    sender_message_handler.handle_request_download_files(*payload, snaps);
                else
                    std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

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

    return 0;
}
