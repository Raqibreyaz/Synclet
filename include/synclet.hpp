#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <format>
#include <chrono>
#include <openssl/sha.h>

#define DEFAULT_CHUNK_SIZE 4096 // 4KB

namespace fs = std::filesystem;
using json = nlohmann::json;

struct ChunkInfo
{
    size_t offset;
    size_t size;
    std::string hash; // e.g., SHA-256
    ChunkInfo();
    ChunkInfo(size_t offset, size_t size, const std::string &hash) {}
};

struct FileSnapshot
{
    std::string path;
    uint64_t size;
    std::time_t mtime;
    std::vector<ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const std::string& path,const uint64_t& size,const std::time_t& mtime,const std::vector<ChunkInfo>& chunks);
};

std::time_t to_unix_timestamp(const fs::file_time_type &mtime);
std::string create_hash(const std::vector<char> &data);

class State
{
public:
    // Scan a directory and build a snapshot of all files and their chunks
    static std::vector<FileSnapshot> scan_directory(const std::string &dir, size_t chunk_size = DEFAULT_CHUNK_SIZE);

    // Compare two snapshots and return changed/added/deleted chunks
    static void compare_snapshots(
        const std::vector<FileSnapshot> &old_snap,
        const std::vector<FileSnapshot> &new_snap,
        std::vector<FileSnapshot> &added,
        std::vector<FileSnapshot> &modified,
        std::vector<FileSnapshot> &deleted);

    // Serialize/deserialize snapshot to/from file
    static bool save_snapshot(const std::string &filename, const std::vector<FileSnapshot> &snaps);
    static bool load_snapshot(const std::string &filename, std::vector<FileSnapshot> &snaps);
};
