// client.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "../include/utils.hpp"
#include "../include/tcp-socket.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

int connecter()
{
    int sock;
    struct sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket failed");
        return 1;
    }

    // 2. Setup address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 3. Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        close(sock);
        return 1;
    }

    std::string handshake = "SYNCLET_HELLO";
    send(sock, handshake.c_str(), handshake.size(), 0);
    std::cout << "📨 Sent: " << handshake << std::endl;

    // 4. Read response
    ssize_t bytes = read(sock, buffer, BUFFER_SIZE - 1);
    buffer[bytes] = '\0';

    std::cout << "📥 Received: " << buffer << std::endl;

    // 5. Cleanup
    close(sock);

    return 0;
}

int main()
{
    auto snaps = load_snapshot(SNAP_FILE);

    if (snaps.empty())
        snaps = scan_directory(DATA_DIR);

    save_snapshot(SNAP_FILE, snaps);

    // TcpConnection client(SERVER_IP, std::to_string(PORT));

    // std::string msg_string = client.receiveAll();

    // std::clog << "request from server: " << msg_string << std::endl;

    // json j;
    // Message msg;

    // // convert to object
    // from_json(msg_string, msg);

    // if (msg.type == Type::REQUEST_SYNC)
    // {

    // }

    // client.closeConnection();

    return 0;
}
