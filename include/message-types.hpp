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
    std::unordered_map<std::string, ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const std::string &filename, const uint64_t &file_size, const std::time_t &mtime, const std::unordered_map<std::string,ChunkInfo> &chunks);
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

struct AddRemoveChunkPayload
{
    std::string filename;
    size_t offset;
    size_t chunk_size;
    bool is_last_chunk;
    AddRemoveChunkPayload();
    AddRemoveChunkPayload(const std::string& filename,const size_t offset,const size_t chunk_size,const bool is_last_chunk);
};

struct ModifiedChunkPayload : public AddRemoveChunkPayload
{
    size_t old_chunk_size;
    ModifiedChunkPayload();
    ModifiedChunkPayload(const std::string& filename,const size_t offset,const size_t chunk_size,const size_t old_chunk_size,const bool is_last_chunk);
};