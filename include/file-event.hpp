#pragma once
#include <string>
#include <chrono>
#include <filesystem>
#include <sys/inotify.h>
#include <optional>

namespace fs = std::filesystem;

enum class EventType
{
    CREATED,
    MODIFIED,
    DELETED,
    MOVED,
    UNKNOWN
};

class FileEvent
{
public:
    std::filesystem::path filepath;

    bool is_directory;

    // for handling rename logic
    std::optional<std::filesystem::path> old_filepath;
    std::optional<std::filesystem::path> new_filepath;

    EventType event_type;
    std::chrono::steady_clock::time_point timestamp;

    // constructor for CREATE, DELETE, MODIFY
    FileEvent(const std::string &filepath,
              const bool is_dir,
              EventType event_type);

    // constructor for rename events
    FileEvent(
        const bool is_dir,
        const std::string &old_filepath,
        const std::string &new_filepath,
        EventType event_type);
};

EventType getEventTypeFromMask(uint32_t mask);