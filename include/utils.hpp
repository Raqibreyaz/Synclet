#pragma once

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <filesystem>
#include <chrono>
#include <unordered_set>
#include <cmath>

// returns the filename from the filepath
std::string extract_filename_from_path(const std::string &dir_to_skip, const std::string &path);

std::string sanitize_filename(const std::string& filename);

// returns the binary encoded string of the number
std::string convert_to_binary_string(size_t n);

std::time_t to_unix_timestamp(const std::filesystem::file_time_type &mtime);

void print_progress_bar(const std::string &message, double progress, size_t bar_width = 30);