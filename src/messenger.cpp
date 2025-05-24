#include <nlohmann/json.hpp>
#include <iostream>
#include "../include/messenger.hpp"
#include "../include/utils.hpp"

using json = nlohmann::json;

Messenger::Messenger(TcpConnection &conn) : client(conn) {}

void Messenger::send_file_data(FileIO &fileio, size_t offset, size_t chunk_size)
{
    std::string chunk_string = fileio.read_file_from_offset(offset, chunk_size);

    // send the message raw data
    client.sendAll(chunk_string);

    // now send EOF to stop waiting
    client.shutdownWrite();
}

void Messenger::send_json_message(const Message &msg)
{

    json j;

    to_json(j, msg);

    const std::string message = j.dump();

    // sending the len of json message
    std::string json_len_string = convert_to_binary_string(message.size());
    client.sendAll(json_len_string);

    // sending the json message
    client.sendAll(message);
}