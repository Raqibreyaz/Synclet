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
    if (snaps.size())
        snaps = scan_directory(DATA_DIR);

    TcpServer server("127.0.0.1", std::to_string(PORT));

    TcpConnection client = server.acceptClient();

    json j;
    Message msg;
    msg.type = Type::REQUEST_SYNC;
    msg.payload = {};

    to_json(j, msg);

    client.sendAll(j.dump());

    std::string snap_string = client.receiveAll();

    std::clog << snap_string << std::endl;

    client.closeConnection();

    return 0;
}
