#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <filesystem>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include "file-event.hpp"
#include "utils.hpp"

namespace fd = std::filesystem;

class Watcher
{
public:
    explicit Watcher(const std::string &dir);
    std::vector<FileEvent> poll_events();
    ~Watcher();

private:
    const int BUFFER_SIZE = 4096;
    const int RENAME_DELAY = 200;
    const int MAX_EVENTS = 10;
    const size_t EVENT_SIZE = sizeof(struct inotify_event);

    int infd;
    int epollfd;
    int timerfd;
    std::string watch_dir;
    void apply_watchers(const std::string &dir);
    void apply_epoll_timer();
    void register_timer();
    void unregister_timer();
    void fill_events(std::vector<FileEvent> &file_events, struct inotify_event *event);

    struct FileMovePair
    {
        std::string old_file_path;
        bool is_directory;
        std::chrono::steady_clock::time_point time_stamp;
        FileMovePair(const std::string &old_file_path,
                     const bool is_dir)
            : old_file_path(old_file_path),
              is_directory(is_dir),
              time_stamp(std::chrono::steady_clock::now()) {}
    };

    std::unordered_map<int, std::string> wd_to_dir;

    std::unordered_map<uint32_t, FileMovePair>
        file_moved_tracker;
};