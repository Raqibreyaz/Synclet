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
    FILE_CREATE,
    FILE_REMOVE,
    FILE_MOVED,
    FILES_CREATE,
    FILES_REMOVE,
    DIR_CREATE,
    DIR_REMOVE,
    DIR_MOVED,
    DIRS_CREATE,
    DIRS_REMOVE,
    REQ_SNAP_VERSION,
    SNAP_VERSION,
    REQ_DIR_LIST,
    DIR_LIST,
    REQ_SNAP,           // client asks server to send snap
    DATA_SNAP,          // server replies with the snap
    REQ_DOWNLOAD_FILES, // client needs to or download files
    SEND_FILE,
    REQ_CHUNK,
    SEND_CHUNK, // for sending entire files by chunk
};

enum class ChunkType : uint8_t
{
    ADD = 0x01,
    REMOVE,
    MODIFY
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

struct SnapVersionPayload
{
    std::string snap_version;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SnapVersionPayload, snap_version);
};

struct DirListPayload{
    std::vector<std::string> dirs;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DirListPayload,dirs);
};

struct FilesCreatedPayload : Files
{
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilesCreatedPayload, files);
};

struct FilesRemovedPayload : Files
{
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilesRemovedPayload, files);
};

struct FileMovedPayload
{
    std::string old_filename;
    std::string new_filename;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileMovedPayload, old_filename, new_filename);
};

struct DirCreateRemovePayload 
{
    std::string dir_path;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DirCreateRemovePayload, dir_path);
};

struct DirMovedPayload
{
    std::string old_dir_path;
    std::string new_dir_path;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DirMovedPayload, old_dir_path, new_dir_path);
};

struct DirsCreatedRemovedPayload
{
    std::vector<std::string> dirs;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DirsCreatedRemovedPayload, dirs);
};

struct ModifiedChunkPayload
{
    ChunkType chunk_type;
    std::string filename;
    size_t offset;
    size_t chunk_size;
    size_t old_chunk_size;
    bool is_last_chunk;
    ModifiedChunkPayload();
    ModifiedChunkPayload(const ChunkType chunk_type, const std::string &filename, const size_t offset, const size_t chunk_size, const size_t old_chunk_size, const bool is_last_chunk);

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModifiedChunkPayload, chunk_type, filename, offset, chunk_size, old_chunk_size, is_last_chunk);
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

struct SendFilePayload
{
    std::string filename;
    size_t file_size;
    int no_of_chunks;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendFilePayload, filename, file_size, no_of_chunks);
};

struct RequestChunkPayload
{
    std::string filename;
    size_t offset;
    size_t chunk_size;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestChunkPayload, filename, offset, chunk_size);
};

// will only required for sending whole files
struct SendChunkPayload
{
    std::string filename;
    size_t chunk_size;
    int chunk_no;
    bool is_last_chunk;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendChunkPayload, filename, chunk_size, chunk_no, is_last_chunk);
};