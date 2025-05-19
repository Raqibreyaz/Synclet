#include <iostream>
#include <sys/inotify.h>
#include <csignal>
#include <stdexcept>
#include <unistd.h>
#include <filesystem>
#include <functional>
#include <format>
// #include "../include/utils.hpp"

#define SNAP_FILE "./snap-file.json"
#define WORKING_DIR "./test"
#define BUFFER_SIZE 4096

namespace fs = std::filesystem;

std::function<void(int)> signal_lambda = nullptr;

void signal_handler(int sig)
{
    if (signal_lambda)
        signal_lambda(sig);
}

// offset_1 : 914
// offset_2 : 914
// size_1 : 429
// size_2 : 430
// [914, 429] changed
// in ./test/file_2.txt

// offset_1 : 0
// offset_2 : 0
// size_1 : 2762
// size_2 : 2763
// [0, 2762] changed
// in./ test / file_20.txt

struct RenameTrack
{
    std::string from;
    std::string to;
};

int main(int argc, char *argv[])
{
    // create an inotify instance
    int infd = inotify_init();
    if (infd == -1)
        throw std::runtime_error("failed to create inotify instance");

    // create a watcher for watching files/directory
    int wd = inotify_add_watch(infd, WORKING_DIR, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1)
        throw std::runtime_error("failed to create watcher");

    // buffer for storing the raw events
    char buffer[BUFFER_SIZE + 1];

    // for storing a particular event
    struct inotify_event *event = nullptr;

    // snapshot so far
    // auto prevSnapshot = load_snapshot(SNAP_FILE);

    // take the snapshot of all the files
    // auto currSnapshot = scan_directory(WORKING_DIR);

    auto fileRenameTracker = std::unordered_map<uint32_t, RenameTrack>();

    // control when ctrl + c pressed
    // signal_lambda = [infd, wd, &currSnapshot, &prevSnapshot, &fileRenameTracker](int sig)
    // {
    //     // closing the watcher
    //     inotify_rm_watch(infd, wd);
    //     // closing the inotify fd
    //     close(infd);

    //     for (auto &[cookie, renameObj] : fileRenameTracker)
    //     {
    //         std::clog << std::format("cookie: {}\noldName: {}\nnewName: {}", cookie, renameObj.from, renameObj.to) << std::endl;
    //     }

    //     compare_snapshots(currSnapshot, prevSnapshot);

    //     save_snapshot(SNAP_FILE, currSnapshot);

    //     exit(EXIT_SUCCESS);
    // };

    signal_lambda = [&fileRenameTracker](int sig)
    {
        for (auto &[cookie, renameObj] : fileRenameTracker)
        {
            std::clog << std::format("cookie: {}\noldName: {}\nnewName: {}", cookie, renameObj.from, renameObj.to) << std::endl;
        }
        exit(EXIT_SUCCESS);
    };

    // when ctrl + c pressed then do this
    signal(SIGINT, signal_handler);

    while (true)
    {
        std::clog << "Waiting for changes..." << std::endl;

        // read the events
        ssize_t bytes_read = read(infd, buffer, BUFFER_SIZE);

        if (bytes_read <= 0)
            continue;

        // now log all the events
        const char *ptr = buffer;
        while (ptr < buffer + bytes_read)
        {
            // extract the event using the ptr
            event = (struct inotify_event *)ptr;

            // move ptr to next event
            ptr += sizeof(struct inotify_event) + event->len;

            // skip if there is no file path
            if (event->len == 0)
                continue;

            // creating filepath
            std::string file_path(std::string(WORKING_DIR) + "/" + event->name);

            // remove the file from track as it is removed from directory
            if (event->mask & IN_DELETE)
            {
                // currSnapshot.erase(file_path);
                std::clog << "Removed: " << file_path << std::endl;
            }

            // if the file is renamed
            else if ((event->mask & IN_MOVED_FROM) || (event->mask & IN_MOVED_TO))
            {
                bool isOldName = event->mask & IN_MOVED_FROM;
                auto &entry = fileRenameTracker[event->cookie];

                if (isOldName)
                    entry.from = event->name;
                else
                    entry.to = event->name;
            }

            // when the file exists then update its snapshot
            else if (fs::exists(file_path))
            {
                // now update the file
                // currSnapshot[file_path] = createSnapshot(file_path,
                //                                          fs::file_size(file_path),
                //                                          to_unix_timestamp(fs::last_write_time(file_path)));
                std::clog << "Updated: " << file_path << std::endl;
            }

            // fallback when either conditions not worked
            else
                std::cerr << "changed file not found " << file_path << std::endl;
        }
    }

    return 0;
}