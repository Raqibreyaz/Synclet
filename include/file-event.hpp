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
    RENAMED,
    UNKNOWN
};

class FileEvent
{
public:
    std::filesystem::path filepath;

    // for handling rename logic
    std::optional<std::filesystem::path> old_filepath;
    std::optional<std::filesystem::path> new_filepath;

    EventType event_type;
    uint32_t cookie;
    std::chrono::steady_clock::time_point timestamp;

    // constructor for CREATE, DELETE, MODIFY
    FileEvent(const std::string &filepath, EventType event_type, uint32_t cookie);

    // constructor for rename events
    FileEvent(const std::string &old_filepath,
              const std::string &new_filepath,
              EventType event_type,
              uint32_t cookie);
};

EventType getEventTypeFromMask(uint32_t mask);