#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <format>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// the type of the data
enum class Type
{
    REQUEST_SYNC,
    SNAPSHOT_SYNC,
    SYNC_REQUIRED,
    SEND_CHUNK
};

// info of chunk
struct ChunkInfo
{
    size_t offset;
    size_t chunk_size;
    std::string hash;
    int chunk_no;
    ChunkInfo();
    ChunkInfo(size_t offset, size_t _chunksize, const std::string &hash,int chunk_no);
};

// a complete snapshot of file
struct FileSnapshot
{
    std::string filename;
    uint64_t file_size;
    std::time_t mtime;
    std::vector<ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const std::string& filename,const uint64_t &file_size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks);
};

// filename and the chunks required from it
struct FileSyncRequired
{
    std::string filename;
    std::vector<int> required_chunks;
    bool is_whole_file_required;
};

// paylaod when sending snapshot
struct SnapshotSyncPayload
{
    std::vector<FileSnapshot> file_snapshots;
};

// payload when requesting required files
struct SyncRequiredPayload
{
    std::vector<FileSyncRequired> required_files;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SyncRequiredPayload, required_files);
};

// payload when sending a chunk data
struct SendChunkPayload
{
    std::string filename;
    int chunk_no;
    size_t chunk_size;
    size_t offset;
    bool is_last_chunk;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendChunkPayload, filename, chunk_no, chunk_size, offset, is_last_chunk)
};

// payload object will be of these types
using PayloadVariant = std::variant<
    std::monostate,
    SnapshotSyncPayload,
    SyncRequiredPayload,
    SendChunkPayload>;

struct Message
{
    Type type;
    PayloadVariant payload;
};

void to_json(json &j, const Message &msg);

void from_json(const json &j, Message &msg);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChunkInfo, offset, chunk_size, hash, chunk_no)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileSnapshot, filename, file_size, mtime, chunks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileSyncRequired, filename, required_chunks, is_whole_file_required)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SnapshotSyncPayload, file_snapshots);