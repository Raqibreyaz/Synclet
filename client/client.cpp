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
    // configuring snap manager and watcher over the working dir
    SnapshotManager snap_manager(DATA_DIR, SNAP_FILE);
    Watcher watcher(DATA_DIR);

    // create connection to server
    TcpConnection client(SERVER_IP, std::to_string(PORT));

    // configuring messenger to send/receive messages
    Messenger messenger(client);

    // configuring change handler to sync changes
    FileChangeHandler file_change_handler(messenger, DATA_DIR);

    // get server's snap and current snap
    auto curr_snap = snap_manager.scan_directory();
    auto server_snap = snap_manager.load_snapshot();

    // for checking if initial changes
    const bool is_server_snap_present = !server_snap.empty();

    // request server to send snap of data
    if (!is_server_snap_present)
    {
        Message msg;

        msg.type = MessageType::REQ_SNAP;
        msg.payload = {};

        messenger.send_json_message(msg);

        // receive snap of data from server
        Message &&server_message = messenger.receive_json_message();

        if (msg.type != MessageType::DATA_SNAP)
            throw std::runtime_error("invalid type of message");

        if (auto payload_ptr = std::get_if<DataSnapshotPayload>(&(server_message.payload)))
        {
            for (auto &file : payload_ptr->files)
                server_snap[file.filename] = std::move(file);
        }
        else
            throw std::runtime_error("invalid data received");
    }

    // compare both snapshots and find changes
    DirChanges &&dir_changes = snap_manager.compare_snapshots(curr_snap, server_snap);

    if (dir_changes.created_files.empty() && dir_changes.modified_files.empty() && dir_changes.removed_files.empty())
        std::clog << "no initial changes found" << std::endl;

    // sync changes
    else if (is_server_snap_present)
    {
        std::clog << "initial changes found!! syncing..." << std::endl;
        file_change_handler.handle_changes(dir_changes);

        // after syncing changes now save the server's snap
        snap_manager.save_snapshot(curr_snap);
    }

    signal_handler = [&client]()
    {
        client.closeConnection();
        exit(EXIT_SUCCESS);
    };
    signal(SIGINT, signal_handler_wrap);

    // continuously watch for changes to sync
    while (true)
    {
        const auto events = watcher.poll_events();

        for (const auto &event : events)
            file_change_handler.handle_event(event, curr_snap);
    }

    return 0;
}