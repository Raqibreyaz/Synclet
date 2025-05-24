// server.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../include/tcp-socket.hpp"
#include "../include/snapshot.hpp"
#include "../include/message.hpp"

#define PORT 9000
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

std::function<void()> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler();
}

int main()
{
    // fetch the snaps from SNAP_FILE
    auto snaps = load_snapshot(SNAP_FILE);

    // if no snap available then create from directory
    snaps = scan_directory(DATA_DIR);
    save_snapshot(SNAP_FILE, snaps);

    // create a server on localhost
    TcpServer server("127.0.0.1", std::to_string(PORT));
    TcpConnection client;

    signal_handler = [&client]()
    {
        client.closeConnection();
        exit(1);
    };

    signal(SIGINT, signal_handler_wrap);

    while (true)
    {
        // accept a client
        client = server.acceptClient();

        json j;
        Message msg;

        std::string json_len_string = client.receiveSome(sizeof(u_int32_t));

        uint32_t json_len;
        memcpy(&json_len, json_len_string.data(), sizeof(json_len));
        json_len = ntohl(json_len);

        std::clog << json_len << " bytes of json" << std::endl;

        std::string json_message = client.receiveSome(json_len);
        std::clog << json_message << std::endl;

        std::clog << "actual data: " << client.receiveAll() << std::endl;
    }
    return 0;
}
