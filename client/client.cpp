#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <sys/inotify.h>
#include <sys/signal.h>
#include "../include/tcp-socket.hpp"
#include "../include/file-io.hpp"
#include "../include/watcher.hpp"
#include "../include/message.hpp"
#include "../include/snapshot-manager.hpp"
#include "../include/messenger.hpp"
#include "../include/file-change-handler.hpp"

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

std::function<void()> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler();
}

int main()
{
    // initially server and client either will have snapshot to get an understanding of what to update on data

    SnapshotManager snap_manager(DATA_DIR, SNAP_FILE);

    // get prev and current snapshots
    auto curr_snap = snap_manager.scan_directory();
    auto prev_snap = snap_manager.load_snapshot();

    DirChanges &&dir_changes = snap_manager.compare_snapshots(curr_snap, prev_snap);

    snap_manager.save_snapshot(curr_snap);

    // create connection to server
    TcpConnection client(SERVER_IP, std::to_string(PORT));

    Watcher watcher(DATA_DIR);

    Messenger messenger(client);

    FileChangeHandler file_change_handler(messenger);

    json j;
    Message msg;

    signal_handler = [&client]()
    {
        client.closeConnection();
        exit(EXIT_SUCCESS);
    };

    signal(SIGINT, signal_handler_wrap);

    // continuously watch for changes to sync
    while (true)
    {
        auto events = watcher.poll_events();

        for (auto &event : events)
            file_change_handler.handle_event(event, prev_snap, curr_snap);
    }

    client.closeConnection();

    return 0;
}
