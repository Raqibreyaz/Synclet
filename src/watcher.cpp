#include "../include/watcher.hpp"

Watcher::Watcher(const std::string &dir) : watch_dir(dir)
{
    infd = inotify_init1(IN_NONBLOCK);
    if (infd == -1)
        throw std::runtime_error("failed to init inotify");

    // applying watchers to the directory recursively
    apply_watchers(watch_dir);

    // applying epoll setup + timer
    apply_epoll_timer();
}

void Watcher::apply_watchers(const std::string &dir)
{
    uint32_t masks = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    // adding watcher to given directory
    int wd = inotify_add_watch(infd, dir.c_str(), masks);
    if (wd == -1)
        throw std::runtime_error(std::format("failed to add watcher to {}", dir));

    // adding the wd to map with the dir
    wd_to_dir[wd] = dir;

    // applying watchers to every sub directory
    for (auto entry : fs::recursive_directory_iterator(dir))
    {
        if (entry.is_directory())
        {
            std::string path = entry.path().string();

            wd = inotify_add_watch(infd, path.c_str(), masks);
            if (wd == -1)
                throw std::runtime_error(std::format("failed to add watcher to {}", path));

            wd_to_dir[wd] = path;
        }
    }
}

void Watcher::apply_epoll_timer()
{
    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        std::string error = std::strerror(errno);
        throw std::runtime_error(std::format("epoll_create: {}", error));
    }

    struct epoll_event infy_event;
    infy_event.events = EPOLLIN;
    infy_event.data.fd = infd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, infd, &infy_event) == -1)
    {
        std::string error = std::strerror(errno);
        throw std::runtime_error(std::format("epoll_ctl: {}", error));
    }

    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 100 * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 100 * 1000000;
    timerfd_settime(timerfd, 0, &its, nullptr);
}

void Watcher::register_timer()
{

    struct epoll_event timer_event;
    timer_event.events = EPOLLIN;
    timer_event.data.fd = timerfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &timer_event) == -1)
    {
        std::string error = std::strerror(errno);
        throw std::runtime_error(std::format("epoll_ctl: {}", error));
    }
}

void Watcher::unregister_timer()
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, timerfd, nullptr) == -1)
    {
        std::string error = std::strerror(errno);
        throw std::runtime_error(std::format("epoll_ctl: {}", error));
    }
}

void Watcher::fill_events(std::vector<FileEvent> &file_events, struct inotify_event *event)
{

    // taking the parent dir where this file event occured
    std::string dir = wd_to_dir[event->wd];

    std::string filepath = extract_filename_from_path(watch_dir, dir + "/" + event->name);

    // check if it is a directory
    const bool is_dir = event->mask & IN_ISDIR ? true : false;

    // ignore mask means directory is deleted then remove it from our track
    if (event->mask & IN_IGNORED)
    {
        wd_to_dir.erase(event->wd);
    }

    //  when file/dir is created
    else if (event->mask & IN_CREATE)
    {
        file_events.emplace_back(
            filepath,
            is_dir,
            EventType::CREATED);

        // if dir added then apply watcher
        if (is_dir)
            apply_watchers(watch_dir + "/" + filepath);

        std::cout << "file create event: " << filepath << std::endl;
    }

    // when file/dir is deleted
    else if (event->mask & IN_DELETE)
    {

        file_events.emplace_back(
            filepath,
            is_dir,
            EventType::DELETED);

        // dir deletion handled in in_ignore case

        std::cout << "file delete event: " << filepath << std::endl;
    }

    // when file is modified
    else if (event->mask & IN_CLOSE_WRITE)
    {
        file_events.emplace_back(filepath,
                                 is_dir,
                                 EventType::MODIFIED);
        std::cout << "file modify event: " << filepath << std::endl;
    }

    // when file/dir is moved from here
    else if (event->mask & IN_MOVED_FROM)
    {
        // register timer when first move event is being added
        if (file_moved_tracker.empty())
        {
            register_timer();
            std::clog << "timer added" << std::endl;
        }

        file_moved_tracker.emplace(event->cookie, FileMovePair(filepath, is_dir));
    }

    // when file/dir is moved here
    else if (event->mask & IN_MOVED_TO)
    {
        auto it = file_moved_tracker.find(event->cookie);

        // when there is no moved_from event happened then it is file/dir is created
        if (it == file_moved_tracker.end())
        {
            file_events.emplace_back(
                filepath,
                is_dir,
                EventType::CREATED);
            std::cout << "file create event: " << filepath << std::endl;

            // applying watcher to the new dir
            if (is_dir)
                apply_watchers(watch_dir + "/" + filepath);

            return;
        }

        // file/dir is renamed/moved
        file_events.emplace_back(
            it->second.is_directory,
            it->second.old_file_path,
            filepath,
            EventType::MOVED);

        // when it is a directory then update wd-to-dir map
        if (it->second.is_directory)
        {
            for (auto &p : wd_to_dir)
            {
                const std::string full_path = std::format("{}/{}", watch_dir, it->second.old_file_path);
                const std::string new_full_path = std::format("{}/{}", watch_dir, filepath);

                if (p.second == full_path)
                {
                    p.second = new_full_path;
                    break;
                }
            }
        }

        std::cout << "file move event: " << it->second.old_file_path << " -> " << filepath << std::endl;

        // now remove the rename entry from map
        file_moved_tracker.erase(it);
    }
}

std::vector<FileEvent> Watcher::poll_events()
{
    // max 10 events
    struct epoll_event events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    // -1 = block until event
    // returns no of events
    int n_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (n_events < 0)
    {
        std::string error = std::strerror(errno);
        throw std::runtime_error(std::format("epoll_wait: {}", error));
    }

    // all the events will be stored in this
    std::vector<FileEvent> file_events;

    // iterate over all the events
    for (int i = 0; i < n_events; i++)
    {
        int fd = events[i].data.fd;

        if (infd == fd)
        {
            int len = read(infd, buffer, BUFFER_SIZE);
            if (len < 0)
            {
                perror("read");
                continue;
            }

            int offset = 0;
            while (offset < len)
            {
                struct inotify_event *event = (struct inotify_event *)&buffer[offset];

                // wd is of the parent dir where watcher applied
                std::string dir = wd_to_dir[event->wd];

                // name of the dir /file created
                std::string file = event->len > 0 ? event->name : "";

                // fullpath including parent folder and file/dir
                std::string full_path = std::format("{}/{}", dir, file);

                // here we have to fill the event array
                fill_events(file_events, event);

                offset += EVENT_SIZE + event->len;
            }
        }

        // when timer expired
        else if (timerfd == fd)
        {
            // remove timer when there is no file move event present
            if (file_moved_tracker.empty())
            {
                unregister_timer();
                std::clog << "timer removed" << std::endl;
                continue;
            }

            uint64_t expirations;
            read(timerfd, &expirations, sizeof(expirations));

            auto now = std::chrono::steady_clock::now();

            for (auto it = file_moved_tracker.begin(); it != file_moved_tracker.end();)
            {

                auto &move_pair = it->second;

                // if >200ms passed for moved file then it is deleted
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - move_pair.time_stamp).count() > 200)
                {
                    file_events.emplace_back(move_pair.old_file_path, move_pair.is_directory,
                                             EventType::DELETED);

                    // remove the entry from wd-to-dir
                    if (move_pair.is_directory)
                    {
                        for (auto &p : wd_to_dir)
                        {
                            const std::string full_path = std::format("{}/{}", watch_dir, it->second.old_file_path);

                            if (p.second == full_path)
                            {
                                wd_to_dir.erase(p.first);
                                break;
                            }
                        }
                    }

                    it = file_moved_tracker.erase(it);
                }
                else
                    it++;
            }
        }
    }

    return file_events;
}

Watcher::~Watcher()
{
    if (epollfd != -1)
        close(epollfd);
    if (timerfd != -1)
        close(timerfd);
    for (auto &[wd, _] : wd_to_dir)
    {
        if (wd != -1)
            inotify_rm_watch(infd, wd);
    }
    if (infd != -1)
        close(infd);
}