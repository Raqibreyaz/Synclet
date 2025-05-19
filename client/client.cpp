// client.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <ranges>
#include <unordered_map>
#include "../include/utils.hpp"
#include "../include/tcp-socket.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

int main()
{
    auto snaps = load_snapshot(SNAP_FILE);

    if (snaps.empty())
    {
        snaps = scan_directory(DATA_DIR);
        save_snapshot(SNAP_FILE, snaps);
    }

    TcpConnection client;

    client.connectToServer(SERVER_IP, std::to_string(PORT));

    std::clog << "waiting for server message" << std::endl;

    std::string server_message = client.receiveAll();

    std::clog << "server message: " << server_message << std::endl;

    json j;
    Message msg, server_msg;

    // convert to object
    from_json(json::parse(server_message), server_msg);

    if (server_msg.type == Type::REQUEST_SYNC)
    {
        std::clog << "sync request received" << std::endl;
        msg.type = Type::SNAPSHOT_SYNC;

        // creating vector for sending filesnapshots
        std::vector<FileSnapshot> values;
        std::transform(snaps.begin(), snaps.end(), std::back_inserter(values),
                       [](const auto &pair)
                       { return pair.second; });

        // storing snapshot as payload
        msg.payload = SnapshotSyncPayload{.file_snapshots = values};

        // converting message to json
        to_json(j, msg);
        std::string message = j.dump();

        std::clog << "sending message size: " << message.size() << std::endl;

        // sending the message
        client.sendAll(message);
    }

    client.closeConnection();

    return 0;
}
