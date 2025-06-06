#pragma once

#include "tcp-socket.hpp"
#include "message.hpp"
#include "file-io.hpp"

class Messenger
{
public:
    explicit Messenger(TcpConnection &conn);

    void send_json_message(const Message &msg) const;
    void send_file_data(FileIO &fileio, const size_t offset, const size_t chunk_size) const;
    void send_file_data(const std::string &data) const;
    Message receive_json_message() const;
    std::string receive_full_data();
    std::string receive_max_given_bytes(size_t max_bytes) const;

private:
    TcpConnection &client;
};
