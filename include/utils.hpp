#pragma once
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
#include <deque>
#include "../include/message.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

std::time_t to_unix_timestamp(const fs::file_time_type &mtime);
std::string create_hash(const std::vector<char> &data);
FileSnapshot createSnapshot(const std::string &file_path, const uint64_t file_size, const time_t last_write_time);

// Scan a directory and build a snapshot of all files and their chunks
std::unordered_map<std::string, FileSnapshot> scan_directory(const std::string &dir);

// Compare two snapshots and return changed/added/deleted chunks
void compare_snapshots(
    const std::unordered_map<std::string, FileSnapshot> &currSnapshot,
    const std::unordered_map<std::string, FileSnapshot> &prevSnapshot);

bool check_file_modification(const FileSnapshot &file_snap_1, const FileSnapshot &file_snap_2);

// Serialize snapshot to file
void save_snapshot(const std::string &filename, const std::unordered_map<std::string, FileSnapshot> &snaps);

// Deserialize snapshot from file
std::unordered_map<std::string, FileSnapshot> load_snapshot(const std::string &filename);
