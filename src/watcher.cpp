#include "../include/watcher.hpp"

Watcher::Watcher(const std::string &dir) : watch_dir(dir)
{
    infd = inotify_init();
    if (infd == -1)
        throw std::runtime_error("failed to init inotify");

    wd = inotify_add_watch(infd, watch_dir.c_str(), IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1)
        throw std::runtime_error("failed to add watcher");
}

std::vector<struct inotify_event *> Watcher::poll_events()
{
    std::vector<struct inotify_event *> events;

    int bytes_read = read(infd, buffer, BUFFER_SIZE);

    // when failed to read events then return empty array
    if (bytes_read <= 0)
        return events;

    const char *ptr = buffer;
    while (ptr < buffer + bytes_read)
    {
        struct inotify_event *event = (struct inotify_event *)ptr;
        ptr += sizeof(struct inotify_event *) + event->len;

        if (event->len == 0)
            continue;   

        events.push_back(event);
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