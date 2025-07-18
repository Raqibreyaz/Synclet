#include "../include/file-event.hpp"

FileEvent::FileEvent(
    const std::string &filepath,
    const bool is_dir,
    EventType event_type)
    : filepath(filepath),
      is_directory(is_dir),
      event_type(event_type),
      timestamp(std::chrono::steady_clock::now())
{
}

FileEvent::FileEvent(
    const bool is_dir,
    const std::string &old_filepath,
    const std::string &new_filepath,
    EventType event_type)
    : is_directory(is_dir),
      old_filepath(fs::path(old_filepath)),
      new_filepath(fs::path(new_filepath)),
      event_type(event_type),
      timestamp(std::chrono::steady_clock::now())
{
}

EventType getEventTypeFromMask(uint32_t mask)
{
    if (mask & IN_CREATE)
        return EventType::CREATED;
    if (mask & IN_DELETE)
        return EventType::DELETED;
    if (mask & IN_CLOSE_WRITE || mask & IN_MODIFY)
        return EventType::MODIFIED;
    if (mask & IN_MOVED_FROM)
        return EventType::MOVED;
    if (mask & IN_MOVED_TO)
        return EventType::MOVED;

    return EventType::UNKNOWN;
}