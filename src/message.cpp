#include "../include/message.hpp"

// converts the message type to string
std::string message_type_to_string(Type type)
{
    switch (type)
    {
    case Type::REQUEST_SYNC:
        return "request_sync";

    case Type::SNAPSHOT_SYNC:
        return "snapshot_sync";

    case Type::SYNC_REQUIRED:
        return "sync_required";

    case Type::SEND_CHUNK:
        return "send_chunk";

    default:
        return "unknown";
    }
}

// converts the string message type to original type
Type message_type_from_string(const std::string &type)
{
    if (type == "request_sync")
        return Type::REQUEST_SYNC;
    else if (type == "snapshot_sync")
        return Type::SNAPSHOT_SYNC;
    else if (type == "sync_required")
        return Type::SYNC_REQUIRED;
    else if (type == "send_chunk")
        return Type::SEND_CHUNK;

    throw std::runtime_error(std::format("unknown message type received {}", type));
}

// converts message to json
void to_json(json &j, const Message &msg)
{
    j["type"] = msg.type;

    std::visit([&](auto &actualPayload)
               {
                using T = std::decay_t <decltype(actualPayload)>;
                if constexpr(!std::is_same_v<T,std::monostate>){
                    j["payload"] = actualPayload;
                } }, msg.payload);
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

    if (m.type == Type::SNAPSHOT_SYNC)
        m.payload = payload_json.get<SnapshotSyncPayload>();
    else if (m.type == Type::REQUEST_SYNC)
        m.payload = payload_json.get<SyncRequiredPayload>();
    else if (m.type == Type::SEND_CHUNK)
        m.payload = payload_json.get<SendChunkPayload>();
    else
        m.payload = std::monostate{};
}