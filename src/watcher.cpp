#include "../include/watcher.hpp"

Watcher::Watcher(const std::string &dir) : watch_dir(std::filesystem::path(dir))
{
    infd = inotify_init();
    if (infd == -1)
        throw std::runtime_error("failed to init inotify");

    wd = inotify_add_watch(infd, watch_dir.string().c_str(), IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1)
        throw std::runtime_error("failed to add watcher");
}

std::vector<FileEvent> Watcher::poll_events()
{
    std::vector<FileEvent> events;

    int bytes_read = read(infd, buffer, BUFFER_SIZE);

    // when failed to read events then return empty array
    if (bytes_read <= 0)
        return events;

    const char *ptr = buffer;
    while (ptr < buffer + bytes_read)
    {
        struct inotify_event *event = (struct inotify_event *)ptr;
        ptr += sizeof(struct inotify_event) + event->len;

        if (event->len == 0)
            continue;

        EventType event_type = getEventTypeFromMask(event->mask);

        std::string filepath = std::format("{}/{}", watch_dir.string(), std::string(event->name));

        // add rename event in the map
        if (event_type == EventType::RENAMED)
        {
            auto rename_it = file_rename_tracker.find(event->cookie);

            // if there is no entry then create entry and point iterator over it
            if (rename_it == file_rename_tracker.end())
            {
                file_rename_tracker[event->cookie].timestamp = std::chrono::steady_clock::now();
                rename_it = file_rename_tracker.find(event->cookie);
            }

            if (event->mask & IN_MOVED_FROM)
                rename_it->second.old_filepath = fs::path(filepath);
            else
                rename_it->second.new_filepath = fs::path(filepath);
        }

        auto now = std::chrono::steady_clock::now();

        // handle rename and expred rename timers
        for (auto rename_it = file_rename_tracker.begin(); rename_it != file_rename_tracker.end(); rename_it++)
        {
            const std::string old_filepath = rename_it->second.old_filepath;
            const std::string new_filepath = rename_it->second.new_filepath;
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - rename_it->second.timestamp);
            bool is_rename_taken = false;

            // when both exists then push the rename event
            if (!(old_filepath.empty()) && !(new_filepath.empty()))
            {
                events.emplace_back(old_filepath, new_filepath, EventType::RENAMED, event->cookie);
                is_rename_taken = true;
            }

            // when moved_from exists and timer expired
            if (!(old_filepath.empty()) && duration.count() >= RENAME_DELAY)
            {
                events.emplace_back(filepath, EventType::DELETED, -1);
                is_rename_taken = true;
            }

            // when moved_to exists and timer expires
            if (!(new_filepath.empty()) && duration.count() >= RENAME_DELAY)
            {
                events.emplace_back(filepath, EventType::CREATED, -1);
                is_rename_taken = true;
            }

            if (is_rename_taken)
                file_rename_tracker.erase(rename_it);
        }

        // when the event is CREATE || DELETE || MODIFIED 
        if (event_type != EventType::RENAMED)
            events.emplace_back(filepath, event_type, event->cookie);
    }

    return events;
}

Watcher::~Watcher()
{
    if (wd == -1)
        inotify_rm_watch(infd, wd);
    if (infd != -1)
        close(infd);
}