#pragma once

#include "tcp-socket.hpp"
#include "message.hpp"
#include "file-io.hpp"

class Messenger
{
public:
   explicit Messenger(TcpConnection& conn);

    void send_json_message(const Message& msg);
    void send_file_data(FileIO &fileio,size_t offset,size_t chunk_size);

private:
    TcpConnection &client;
};
