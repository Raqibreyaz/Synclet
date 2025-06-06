#pragma once
#include <string>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <format>
#include <chrono>
#include <openssl/sha.h>
#include <deque>
#include "../include/utils.hpp"
#include "../include/message-types.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using DirSnapshot = std::unordered_map<std::string, FileSnapshot>;

struct FileModification
{
    std::string filename;
    std::vector<ModifiedChunkPayload> modified_chunks;
};

struct DirChanges
{
    std::vector<std::string> removed_files;
    std::vector<FileModification> modified_files;
    std::vector<FileSnapshot> created_files;
};

class SnapshotManager
{
public:
    SnapshotManager(const std::string &data_dir_path, const std::string &snap_file_path);

    static FileSnapshot createSnapshot(const std::string &file_path);

    // Scan a directory and build a snapshot of all files and their chunks
    std::pair<std::string, DirSnapshot> scan_directory();

    // Compare two snapshots and return changed/added/deleted chunks
    DirChanges compare_snapshots(
        const DirSnapshot &currSnapshot,
        const DirSnapshot &prevSnapshot);

    static FileModification get_file_modification(const FileSnapshot &file_curr_snap, const FileSnapshot &file_prev_snap);

    // Serialize snapshot to file
    void save_snapshot(const DirSnapshot &snaps);

    // Deserialize snapshot from file
    std::pair<std::string, DirSnapshot> load_snapshot();

private:
    fs::path data_dir_path;
    fs::path snap_file_path;
    static std::string create_hash(const std::vector<char> &data);
};
