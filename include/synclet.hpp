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
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct ChunkInfo
{
    size_t offset;
    size_t size;
    std::string hash; // e.g., SHA-256
    ChunkInfo();
    ChunkInfo(size_t offset, size_t size, const std::string &hash);
};

struct FileSnapshot
{
    uint64_t size;
    std::time_t mtime;
    std::vector<ChunkInfo> chunks;

    FileSnapshot();
    FileSnapshot(const uint64_t &size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks);
};

std::time_t to_unix_timestamp(const fs::file_time_type &mtime);
std::string create_hash(const std::vector<char> &data);
FileSnapshot createSnapshot(const std::string &file_path, const uint64_t file_size, const time_t last_write_time);

class State
{
public:
    // Scan a directory and build a snapshot of all files and their chunks
    static std::unordered_map<std::string, FileSnapshot> scan_directory(const std::string &dir);

    // Compare two snapshots and return changed/added/deleted chunks
    static void compare_snapshots(
        const std::unordered_map<std::string, FileSnapshot> &currSnapshot,
        const std::unordered_map<std::string, FileSnapshot> &prevSnapshot);

    static bool check_file_modification(const FileSnapshot &file_snap_1, const FileSnapshot &file_snap_2);

    // Serialize/deserialize snapshot to/from file
    static void save_snapshot(const std::string &filename, const std::unordered_map<std::string, FileSnapshot> &snaps);
    static std::unordered_map<std::string, FileSnapshot> load_snapshot(const std::string &filename);
};
