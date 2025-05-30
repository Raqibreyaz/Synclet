#include <nlohmann/json.hpp>
#include <iostream>
#include "../include/messenger.hpp"
#include "../include/utils.hpp"

using json = nlohmann::json;

Messenger::Messenger(TcpConnection &conn) : client(conn) {}

void Messenger::send_file_data(FileIO &fileio, const size_t offset, const size_t chunk_size) const
{
    std::string chunk_string = fileio.read_file_from_offset(offset, chunk_size);

    // send the message raw data
    client.sendAll(chunk_string);

    // now send EOF to stop waiting
    // client.shutdownWrite();
}

void Messenger::send_json_message(const Message &msg) const
{
    json j;

    to_json(j, msg);

    const std::string message = j.dump();

    // sending the len of json message
    std::string json_len_string = convert_to_binary_string(message.size());
    client.sendAll(json_len_string);

    // sending the json message
    client.sendAll(message);

    // if (msg.type != MessageType::ADDED_CHUNK && msg.type != MessageType::MODIFIED_CHUNK)
    //     client.shutdownWrite();
}

Message Messenger::receive_json_message() const
{
    json j;
    Message msg;

    // receive and parse the length of message
    std::string json_len_string = client.receiveSome(sizeof(uint32_t));
    uint32_t json_len;
    memcpy(&json_len, json_len_string.data(), sizeof(json_len));
    json_len = ntohl(json_len);

    // receive exact message bytes
    std::string message = client.receiveSome(json_len);

    // parse the json string to json object
    j = json::parse(message);

    // parse json string to normal message object
    from_json(j, msg);

    return msg;
}