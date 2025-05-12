#include <iostream>
#include <sys/inotify.h>
#include <stdexcept>
#include <unistd.h>
#include <filesystem>
#define BUFFER_SIZE 4096

namespace fs = std::filesystem;

void print(const char *arr, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        std::clog << arr[i];
    }
    std::clog << std::endl;
}

int main(int argc, char const *argv[])
{
    // create an inotify instance
    int infd = inotify_init();

    if (infd == -1)
        throw std::runtime_error("failed to create inotify instance");

    // create a watcher for watching files/directory
    int wd = inotify_add_watch(infd, "./test", IN_MODIFY | IN_CREATE | IN_DELETE);

    if (wd == -1)
        throw std::runtime_error("failed to create watcher");

    // buffer for storing the raw events
    char buffer[BUFFER_SIZE + 1];
    struct inotify_event *event = nullptr;

    while (true)
    {
        std::clog << "Waiting for changes..." << std::endl;
        // read the events
        size_t bytes_read = read(infd, buffer, BUFFER_SIZE);

        std::clog << "event buffer: " << std::endl;
        print(buffer, bytes_read);

        // now log all the events
        const char *ptr = buffer;
        while (ptr < buffer + bytes_read)
        {
            // extract the event using the ptr
            event = (struct inotify_event *)ptr;
            // move ptr to next event
            ptr += sizeof(struct inotify_event) + event->len;

            if (event->mask & IN_MODIFY)
                std::clog << event->name << " file is modified" << std::endl;
            else if (event->mask & IN_CREATE)
                std::clog << event->name << " file is created" << std::endl;
            else if (event->mask & IN_DELETE)
                std::clog << event->name << " file is deleted" << std::endl;
        }
    }

    // closing the watcher
    inotify_rm_watch(infd, wd);
    // closing the inotify fd
    close(infd);
    return 0;
}
