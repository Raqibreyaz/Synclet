#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <format>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// the type of the payload
enum class MessageType
{
    SEND_CHUNK,
    MODIFIED_CHUNK,
    ADDED_CHUNK,
    REMOVED_CHUNK,
    FILE_CREATE,
    FILE_REMOVE,
    FILE_RENAME
};

// info of chunk
struct ChunkInfo
{
    size_t offset;
    size_t chunk_size;
    std::string hash;
    int chunk_no;
    ChunkInfo();
    ChunkInfo(size_t offset, size_t _chunksize, const std::string &hash, int chunk_no);
};

// a complete snapshot of file
struct FileSnapshot
{
    std::string filename;
    uint64_t file_size;
    std::time_t mtime;
    std::vector<ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const std::string &filename, const uint64_t &file_size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks);
};

struct FileCreateRemovePayload
{
    std::string filename;
};

struct FileRenamePayload
{
    std::string old_filename;
    std::string new_filename;
};

// payload when sending a chunk data
struct SendChunkPayload
{
    std::string filename;
    int chunk_no;
    size_t chunk_size;
    size_t offset;
};

struct AddChunkPayload
{
    std::string filename;
    size_t new_start_index;
    size_t new_end_index;
    bool is_last_chunk;
};

struct ModifiedChunkPayload : public AddChunkPayload
{
    size_t old_start_index;
    size_t old_end_index;
};

struct TruncateFilePayload
{
    std::string filename;
    size_t last_index;
};
