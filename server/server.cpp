// server.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/utils.hpp"
#include "../include/tcp-socket.hpp"

#define PORT 9000
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

int server()
{
    return 0;
}

int main()
{
    // fetch the snaps from SNAP_FILE
    auto snaps = load_snapshot(SNAP_FILE);

    // if no snap available then create from directory
    if (snaps.empty())
    {
        snaps = scan_directory(DATA_DIR);
        save_snapshot(SNAP_FILE, snaps);
    }

    // create a server on localhost
    TcpServer server("127.0.0.1", std::to_string(PORT));

    // accept a client
    TcpConnection client = server.acceptClient();

    json j;
    Message msg, client_msg;

    // request client to send snapshot
    msg.type = Type::REQUEST_SYNC;
    msg.payload = {};

    // convert request message to json
    to_json(j, msg);

    // send message to client as stringified json
    client.sendAll(j.dump());

    // receive message from client(json string)
    std::string client_message = client.receiveAll();

    std::clog << "received message size: " << client_message.size() << std::endl;

    // convert stringified json to json object to message object
    from_json(json::parse(client_message), client_msg);

    if (client_msg.type == Type::SNAPSHOT_SYNC)
    {
        std::clog << "client has sent snapshots";
    }

    auto payload = std::get<SnapshotSyncPayload>(client_msg.payload);

    auto client_snaps = std::unordered_map<std::string, FileSnapshot>();

    std::transform(payload.file_snapshots.begin(),
                   payload.file_snapshots.end(),
                   std::inserter(client_snaps, client_snaps.end()),
                   [](const FileSnapshot &snapshot)
                   {
                       return make_pair(snapshot.filename, snapshot);
                   });

    compare_snapshots(client_snaps, snaps);

    client.closeConnection();

    return 0;
}
