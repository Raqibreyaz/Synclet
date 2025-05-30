#pragma once

#include "message-types.hpp"

// payload object will be of these types
using PayloadVariant = std::variant<
    std::monostate,
    ModifiedChunkPayload,
    AddRemoveChunkPayload,
    FileCreateRemovePayload,
    FileRenamePayload,
    FilesCreatedPayload,
    FilesRemovedPayload,
    DataSnapshotPayload,
    RequestDownloadFilesPayload,
    SendChunkPayload
    >;

struct Message
{
    MessageType type;
    PayloadVariant payload;
};

void to_json(json &j, const Message &msg);

void from_json(const json &j, Message &msg);

std::string message_type_to_string(MessageType type);

MessageType message_type_from_string(const std::string& type);
