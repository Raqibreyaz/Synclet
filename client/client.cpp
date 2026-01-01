#include <iostream>
#include <cstring>
#include <cstdlib>
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
#include "../include/initial-sync-handler.hpp"

#define PORT 9000
#define SERVER_URL "127.0.0.1"
#define DATA_DIR "client/data"
#define PEER_SNAP_FILE "peer-snap-file.json"

std::function<void(int)> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler(sig);
}

int main(int argc, const char *argv[])
{
    const std::string user_home = std::getenv("HOME");
    const std::unordered_map<std::string, std::string> cli_args = parse_cli_args(argc, argv);

    const std::string peer_snap_filepath = user_home + "/" + PEER_SNAP_FILE;
    const std::string data_dir_path = cli_args.contains("datadir") ? cli_args.at("datadir") : DATA_DIR;
    const std::string server_url = cli_args.contains("server") ? cli_args.at("server") : SERVER_URL;
    const std::string server_port = cli_args.contains("port") ? cli_args.at("port") : std::to_string(PORT);

    // create the data dir if not exists
    if (!fs::exists(data_dir_path))
        fs::create_directory(data_dir_path);

    try
    {
        // configuring snap manager and watcher over the working dir
        SnapshotManager snap_manager(data_dir_path, peer_snap_filepath);

        // create connection to server
        TcpConnection client(server_url, server_port);

        // configuring messenger to send/receive messages
        Messenger messenger(client);

        // configuring sending message handler to sync changes
        SenderMessageHandler sender_message_handler(messenger, data_dir_path);

        // configuring receiving message handler to request & receive message
        ReceiverMessageHandler receiver_message_handler(data_dir_path, messenger);

        // get current snap and peer's snap
        auto [curr_snap_version, curr_snap] = snap_manager.scan_directory();
        auto [peer_snap_version, peer_snap] = snap_manager.load_snapshot();

        // initial changes handled here
        {
            InitialSyncHandler initial_sync_handler(snap_manager, receiver_message_handler, sender_message_handler);
            initial_sync_handler.sync(curr_snap, peer_snap, peer_snap_version, curr_snap_version);
        }

        signal_handler = [&](int _)
        {
            client.closeConnection();
            snap_manager.save_snapshot(curr_snap);
            exit(EXIT_SUCCESS);
        };
        signal(SIGINT, signal_handler_wrap);

        // continuously watch for changes to sync
        Watcher watcher(data_dir_path);
        while (true)
        {
            std::clog << "waiting for file changes..." << std::endl;
            const auto &events = watcher.poll_events();

            for (const auto &event : events)
            {
                std::cout
                    << "filepath: " << event.filepath << std::endl
                    << "is_directory: " << event.is_directory << std::endl;

                sender_message_handler.handle_event(event, curr_snap);
                snap_manager.save_snapshot(curr_snap);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}