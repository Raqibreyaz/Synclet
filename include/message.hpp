#pragma once

#include "message-types.hpp"

// payload object will be of these types
using PayloadVariant = std::variant<
    std::monostate,
    SendChunkPayload,
    ModifiedChunkPayload,
    AddChunkPayload,
    TruncateFilePayload,
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChunkInfo, offset, chunk_size, hash, chunk_no);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileSnapshot, filename, file_size, mtime, chunks);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileCreateRemovePayload, filename);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileRenamePayload, old_filename, new_filename);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SendChunkPayload, filename, chunk_no, chunk_size, offset);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AddChunkPayload, filename, new_start_index, new_end_index,is_last_chunk);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModifiedChunkPayload, filename, old_start_index, old_end_index, new_start_index, new_end_index,is_last_chunk);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TruncateFilePayload, filename, last_index);