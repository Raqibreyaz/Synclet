#pragma once

#include "message-types.hpp"

// payload object will be of these types
using PayloadVariant = std::variant<
    std::monostate,
    ModifiedChunkPayload,
    AddRemoveChunkPayload,
    FileCreateRemovePayload,
    FileRenamePayload>;

struct Message
{
    MessageType type;
    PayloadVariant payload;
};

void to_json(json &j, const Message &msg);

void from_json(const json &j, Message &msg);

std::string message_type_to_string(MessageType type);

MessageType message_type_from_string(const std::string& type);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileCreateRemovePayload, filename);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileRenamePayload, old_filename, new_filename);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AddRemoveChunkPayload, filename, offset,chunk_size,is_last_chunk);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModifiedChunkPayload, filename, offset,chunk_size,old_chunk_size,is_last_chunk);
