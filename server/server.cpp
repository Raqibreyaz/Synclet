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
#include "../include/server-message-handler.hpp"

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

    // fetch the snaps from SNAP_FILE
    auto &&[snap_version, snaps] = snap_manager.load_snapshot();

    // create and save snaps when no snaps present
    if (snaps.empty())
    {
        auto &&p = snap_manager.scan_directory();
        snap_version = p.first;
        snaps = p.second;

        snap_manager.save_snapshot(snaps);
    }

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

    ServerMessageHandler message_handler(DATA_DIR, client);

    while (true)
    {
        Message &&msg = messenger.receive_json_message();

        std::clog << "message type is: " << message_type_to_string(msg.type);

        switch (msg.type)
        {
        case MessageType::FILE_CREATE:
        {
            auto payload = std::get_if<FileCreateRemovePayload>(&(msg.payload));
            if (payload)
                message_handler.process_create_file(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::FILE_REMOVE:
        {
            auto payload = std::get_if<FileCreateRemovePayload>(&(msg.payload));
            if (payload)
                message_handler.process_delete_file(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::FILES_CREATE:
        {
            auto payload = std::get_if<FilesCreatedPayload>(&(msg.payload));

            if (payload)
                message_handler.process_create_file(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);

            break;
        }

        case MessageType::FILES_REMOVE:
        {
            auto payload = std::get_if<FilesRemovedPayload>(&(msg.payload));
            if (payload)
                message_handler.process_delete_file(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::FILE_RENAME:
        {
            auto payload = std::get_if<FileRenamePayload>(&(msg.payload));
            if (payload)
                message_handler.process_file_rename(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::MODIFIED_CHUNK:
        {
            auto payload = std::get_if<ModifiedChunkPayload>(&(msg.payload));
            if (payload)
                message_handler.process_modified_chunk(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::ADDED_CHUNK:
        {
            auto payload = std::get_if<AddRemoveChunkPayload>(&(msg.payload));
            if (payload)
                message_handler.process_add_remove_chunk(*payload, false);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::REMOVED_CHUNK:
        {
            auto payload = std::get_if<AddRemoveChunkPayload>(&(msg.payload));
            if (payload)
                message_handler.process_add_remove_chunk(*payload, true);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::SEND_CHUNK:
        {
            auto payload = std::get_if<SendChunkPayload>(&(msg.payload));
            if (payload)
                message_handler.process_file_chunk(*payload);
            else
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
            break;
        }

        case MessageType::REQ_SNAP:
        {
            DataSnapshotPayload payload;
            payload.files.reserve(snaps.size());

            for (const auto &[_, file_snap] : snaps)
                payload.files.push_back(file_snap);

            msg.type = MessageType::DATA_SNAP;
            msg.payload = std::move(payload);

            messenger.send_json_message(msg);

            break;
        }

        case MessageType::REQ_DOWNLOAD_FILES:
        {
            auto payload = std::get_if<RequestDownloadFilesPayload>(&(msg.payload));

            if (!payload)
            {
                std::cerr << "invalid payload for: " << message_type_to_string(msg.type);
                break;
            }

            // payload->files;
        }

        default:
            break;
        }
    }
    return 0;
}
