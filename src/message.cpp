#include "../include/message.hpp"

// converts the message type to string
std::string message_type_to_string(MessageType type)
{
    switch (type)
    {
    case MessageType::FILE_CREATE:
        return "FILE_CREATE";
    case MessageType::FILE_REMOVE:
        return "FILE_REMOVE";
    case MessageType::FILE_RENAME:
        return "FILE_RENAME";
    case MessageType::MODIFIED_CHUNK:
        return "MODIFIED_CHUNK";
    case MessageType::REMOVED_CHUNK:
        return "REMOVED_CHUNK";
    case MessageType::ADDED_CHUNK:
        return "ADDED_CHUNK";

    default:
        return "UNKNOWN";
    }
}

// converts the string message type to original type
MessageType message_type_from_string(const std::string &type)
{
    if (type == "MODIFIED_CHUNK")
        return MessageType::MODIFIED_CHUNK;
    else if (type == "REMOVED_CHUNK")
        return MessageType::REMOVED_CHUNK;
    else if (type == "FILE_CREATE")
        return MessageType::FILE_CREATE;
    else if (type == "FILE_REMOVE")
        return MessageType::FILE_REMOVE;
    else if (type == "FILE_RENAME")
        return MessageType::FILE_RENAME;
    else if (type == "ADDED_CHUNK")
        return MessageType::ADDED_CHUNK;

    throw std::runtime_error(std::format("unknown message type received {}", type));
}

// converts message to json
void to_json(json &j, const Message &msg)
{
    std::clog << "converting message to json" << std::endl;
    j["type"] = message_type_to_string(msg.type);

    std::clog << "type: " << j["type"] << std::endl;

    std::visit([&](auto &actualPayload)
               {
                // taking type of actualPayload by removing &, const from it
                using T = std::decay_t <decltype(actualPayload)>;

                // if type is not std::monostate(blank) then take the payload
                if constexpr(!std::is_same_v<T,std::monostate>)
                    j["payload"] = actualPayload; },
               msg.payload);

    std::clog << "payload: " << j["payload"] << std::endl;
}

// converts given json to message
void from_json(const json &j, Message &m)
{
    m.type = message_type_from_string(j["type"].get<std::string>());

    if (!j.contains("payload"))
    {
        m.payload = std::monostate{};
        return;
    }

    const auto &payload_json = j["payload"];

    if (m.type == MessageType::FILE_CREATE)
        m.payload = payload_json.get<FileCreateRemovePayload>();
    else if (m.type == MessageType::FILE_REMOVE)
        m.payload = payload_json.get<FileCreateRemovePayload>();
    else if (m.type == MessageType::FILE_RENAME)
        m.payload = payload_json.get<FileRenamePayload>();
    else if (m.type == MessageType::MODIFIED_CHUNK)
        m.payload = payload_json.get<ModifiedChunkPayload>();
    else if (m.type == MessageType::REMOVED_CHUNK)
        m.payload = payload_json.get<AddRemoveChunkPayload>();
    else if (m.type == MessageType::ADDED_CHUNK)
        m.payload = payload_json.get<AddRemoveChunkPayload>();
    else
        m.payload = std::monostate{};
}