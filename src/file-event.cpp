#include "file-event.hpp"

FileEvent::FileEvent(const std::string &filepath, EventType event_type, uint32_t cookie) : filepath(filepath), event_type(event_type), cookie(cookie), timestamp(std::chrono::steady_clock::now())
{
}

FileEvent::FileEvent(const std::string &old_filepath,
                     const std::string &new_filepath,
                     EventType event_type,
                     uint32_t cookie)
    : old_filepath(fs::path(old_filepath)),
      new_filepath(fs::path(new_filepath)),
      event_type(event_type),
      cookie(cookie),
      timestamp(std::chrono::steady_clock::now())
{
}

inline EventType getEventTypeFromMask(uint32_t mask)
{
    if (mask & IN_CREATE)
        return EventType::CREATED;
    if (mask & IN_DELETE)
        return EventType::DELETED;
    if (mask & IN_CLOSE_WRITE)
        return EventType::MODIFIED;
    if (mask & IN_MOVED_FROM)
        return EventType::RENAMED;
    if (mask & IN_MOVED_TO)
        return EventType::RENAMED;

    return EventType::UNKNOWN;
}