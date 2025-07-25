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
    case MessageType::FILE_MOVED:
        return "FILE_MOVED";
    case MessageType::DIR_CREATE:
        return "DIR_CREATE";
    case MessageType::DIR_REMOVE:
        return "DIR_REMOVE";
    case MessageType::DIR_MOVED:
        return "DIR_MOVED";
    case MessageType::DIRS_CREATE:
        return "DIRS_CREATE";
    case MessageType::DIRS_REMOVE:
        return "DIRS_REMOVE";
    case MessageType::MODIFIED_CHUNK:
        return "MODIFIED_CHUNK";
        // case MessageType::ADDED
        // return "ADDED_CHUNK";
    case MessageType::DATA_SNAP:
        return "DATA_SNAP";
    case MessageType::FILES_CREATE:
        return "FILES_CREATE";
    case MessageType::FILES_REMOVE:
        return "FILES_REMOVE";
    case MessageType::REQ_SNAP_VERSION:
        return "REQ_SNAP_VERSION";
    case MessageType::SNAP_VERSION:
        return "SNAP_VERSION";
    case MessageType::REQ_DIR_LIST:
        return "REQ_DIR_LIST";
    case MessageType::DIR_LIST:
        return "DIR_LIST";
    case MessageType::REQ_SNAP:
        return "REQ_SNAP";
    case MessageType::SEND_FILE:
        return "SEND_FILE";
    case MessageType::REQ_CHUNK:
        return "REQ_CHUNK";
    case MessageType::SEND_CHUNK:
        return "SEND_CHUNK";
    case MessageType::REQ_DOWNLOAD_FILES:
        return "REQ_DOWNLOAD_FILES";

    default:
        return "UNKNOWN";
    }
}

// converts the string message type to original type
MessageType message_type_from_string(const std::string &type)
{
    if (type == "MODIFIED_CHUNK")
        return MessageType::MODIFIED_CHUNK;
    else if (type == "FILE_CREATE")
        return MessageType::FILE_CREATE;
    else if (type == "FILE_REMOVE")
        return MessageType::FILE_REMOVE;
    else if (type == "FILE_MOVED")
        return MessageType::FILE_MOVED;
    else if (type == "DIR_CREATE")
        return MessageType::DIR_CREATE;
    else if (type == "DIR_REMOVE")
        return MessageType::DIR_REMOVE;
    else if (type == "DIR_MOVED")
        return MessageType::DIR_MOVED;
    else if (type == "DIRS_CREATE")
        return MessageType::DIRS_CREATE;
    else if (type == "DIRS_REMOVE")
        return MessageType::DIRS_REMOVE;
    else if (type == "DATA_SNAP")
        return MessageType::DATA_SNAP;
    else if (type == "FILES_CREATE")
        return MessageType::FILES_CREATE;
    else if (type == "FILES_REMOVE")
        return MessageType::FILES_REMOVE;
    else if (type == "REQ_SNAP_VERSION")
        return MessageType::REQ_SNAP_VERSION;
    else if (type == "SNAP_VERSION")
        return MessageType::SNAP_VERSION;
    else if (type == "REQ_DIR_LIST")
        return MessageType::REQ_DIR_LIST;
    else if (type == "DIR_LIST")
        return MessageType::DIR_LIST;
    else if (type == "REQ_SNAP")
        return MessageType::REQ_SNAP;
    else if (type == "SEND_FILE")
        return MessageType::SEND_FILE;
    else if (type == "REQ_CHUNK")
        return MessageType::REQ_CHUNK;
    else if (type == "SEND_CHUNK")
        return MessageType::SEND_CHUNK;
    else if (type == "REQ_DOWNLOAD_FILES")
        return MessageType::REQ_DOWNLOAD_FILES;

    throw std::runtime_error(std::format("unknown message type received {}", type));
}

// converts message to json
void to_json(json &j, const Message &msg)
{
    j["type"] = message_type_to_string(msg.type);

    std::visit([&](auto &actualPayload)
               {
                // taking type of actualPayload by removing &, const from it
                using T = std::decay_t <decltype(actualPayload)>;

                // if type is not std::monostate(blank) then take the payload
                if constexpr(!std::is_same_v<T,std::monostate>)
                    j["payload"] = actualPayload; },
               msg.payload);
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

    switch (m.type)
    {
    case MessageType::REQ_SNAP_VERSION:
        m.payload = std::monostate{};
        break;

    case MessageType::SNAP_VERSION:
        m.payload = payload_json.get<SnapVersionPayload>();
        break;

    case MessageType::REQ_DIR_LIST:
        m.payload = std::monostate{};
        break;

    case MessageType::DIR_LIST:
        m.payload = payload_json.get<DirListPayload>();
        break;

    case MessageType::REQ_SNAP:
        m.payload = std::monostate{};
        break;

    case MessageType::FILE_CREATE:
        m.payload = payload_json.get<FileCreateRemovePayload>();
        break;

    case MessageType::FILE_REMOVE:
        m.payload = payload_json.get<FileCreateRemovePayload>();
        break;

    case MessageType::FILE_MOVED:
        m.payload = payload_json.get<FileMovedPayload>();
        break;

    case MessageType::DIR_CREATE:
        m.payload = payload_json.get<DirCreateRemovePayload>();
        break;

    case MessageType::DIR_REMOVE:
        m.payload = payload_json.get<DirCreateRemovePayload>();
        break;

    case MessageType::DIR_MOVED:
        m.payload = payload_json.get<DirMovedPayload>();
        break;

    case MessageType::DIRS_CREATE:
        m.payload = payload_json.get<DirsCreatedRemovedPayload>();
        break;

    case MessageType::DIRS_REMOVE:
        m.payload = payload_json.get<DirsCreatedRemovedPayload>();
        break;

    case MessageType::MODIFIED_CHUNK:
        m.payload = payload_json.get<ModifiedChunkPayload>();
        break;

    case MessageType::DATA_SNAP:
        m.payload = payload_json.get<DataSnapshotPayload>();
        break;

    case MessageType::FILES_CREATE:
        m.payload = payload_json.get<FilesCreatedPayload>();
        break;

    case MessageType::FILES_REMOVE:
        m.payload = payload_json.get<FilesRemovedPayload>();
        break;

    case MessageType::SEND_FILE:
        m.payload = payload_json.get<SendFilePayload>();
        break;

    case MessageType::REQ_CHUNK:
        m.payload = payload_json.get<RequestChunkPayload>();
        break;

    case MessageType::SEND_CHUNK:
        m.payload = payload_json.get<SendChunkPayload>();
        break;

    case MessageType::REQ_DOWNLOAD_FILES:
        m.payload = payload_json.get<RequestDownloadFilesPayload>();
        break;

    default:
        m.payload = std::monostate{};
    }
}