#pragma once

#include "tcp-socket.hpp"
#include "message.hpp"
#include "file-io.hpp"

class Messenger
{
public:
   explicit Messenger(TcpConnection& conn);

    void send_json_message(const Message& msg);
    void send_file_data(FileIO &fileio,const size_t offset,const size_t chunk_size);
    Message receive_json_message();
    // void receive_file_data();
private:
    TcpConnection &client;
};
