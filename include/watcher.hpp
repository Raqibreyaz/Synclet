#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <sys/inotify.h>
#include <filesystem>
#include <unordered_map>
#include "file-event.hpp"

class Watcher
{
public:
    explicit Watcher(const std::string &dir);
    std::vector<FileEvent> poll_events();
    ~Watcher();

private:
    #define BUFFER_SIZE 4096
    #define RENAME_DELAY 200

    int infd;
    int wd;
    std::filesystem::path watch_dir;

    struct RenamePair
    {
        std::filesystem::path old_filepath;
        std::filesystem::path new_filepath;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<uint32_t, RenamePair> file_rename_tracker;
    char buffer[BUFFER_SIZE];
};