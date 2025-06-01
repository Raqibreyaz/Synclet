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
    FILE_RENAME,
    FILES_CREATE,
    FILES_REMOVE,
    REQ_SNAP_VERSION,
    SNAP_VERSION,
    REQ_SNAP,           // client asks server to send snap
    DATA_SNAP,          // server replies with the snap
    REQ_DOWNLOAD_FILES, // client needs to or download files
    SEND_CHUNK          // for sending entire files by chunk
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

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ChunkInfo, offset, chunk_size, hash, chunk_no);
};

// a complete snapshot of file
struct FileSnapshot
{
    std::string filename;
    uint64_t file_size;
    std::time_t mtime;
    std::unordered_map<std::string, ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const std::string &filename, const uint64_t &file_size, const std::time_t &mtime, const std::unordered_map<std::string, ChunkInfo> &chunks);

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileSnapshot, filename, file_size, mtime, chunks);
};

struct FileCreateRemovePayload
{
    std::string filename;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileCreateRemovePayload, filename);
};

struct Files
{
    std::vector<std::string> files;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Files, files);
};

struct SnapVersionPayload{
    std::string snap_version;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SnapVersionPayload,snap_version);
};

struct FilesCreatedPayload : Files
{
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilesCreatedPayload, files);
};

struct FilesRemovedPayload : Files
{
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilesRemovedPayload, files);
};

struct FileRenamePayload
{
    std::string old_filename;
    std::string new_filename;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileRenamePayload, old_filename, new_filename);
};

struct AddRemoveChunkPayload
{
    std::string filename;
    size_t offset;
    size_t chunk_size;
    bool is_last_chunk;
    AddRemoveChunkPayload();
    AddRemoveChunkPayload(const std::string &filename, const size_t offset, const size_t chunk_size, const bool is_last_chunk);

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AddRemoveChunkPayload, filename, offset, chunk_size, is_last_chunk);
};

struct ModifiedChunkPayload : AddRemoveChunkPayload
{
    size_t old_chunk_size;
    ModifiedChunkPayload();
    ModifiedChunkPayload(const std::string &filename, const size_t offset, const size_t chunk_size, const size_t old_chunk_size, const bool is_last_chunk);

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModifiedChunkPayload, filename, offset, chunk_size, old_chunk_size, is_last_chunk);
};

// sends snapshot of all the files
struct DataSnapshotPayload
{
    std::vector<FileSnapshot> files;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DataSnapshotPayload, files);
};

// will required when initially client hasn't any data
struct RequestDownloadFilesPayload : Files
{
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestDownloadFilesPayload, files);
};

// will only required for sending whole files
struct SendChunkPayload
{
    std::string filename;
    size_t chunk_size;
    int chunk_no;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendChunkPayload, filename, chunk_size, chunk_no);
};