#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <sys/inotify.h>

#define BUFFER_SIZE 4096

class Watcher
{
public:
    explicit Watcher(const std::string &dir);
    std::vector<struct inotify_event*> poll_events();
    ~Watcher();

private:
    int infd;
    int wd;
    std::string watch_dir;
    char buffer[BUFFER_SIZE + 1];
};