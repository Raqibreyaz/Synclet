#pragma once

#include "snapshot-manager.hpp"
#include "sender-message-handler.hpp"
#include "receiver-message-handler.hpp"
#include "message.hpp"
#include <vector>
#include <string>

class InitialSyncHandler
{
private:
    SnapshotManager &snap_manager;
    ReceiverMessageHandler &receiver_handler_;
    SenderMessageHandler &sender_handler_;

public:
    InitialSyncHandler(SnapshotManager &snap_manager,
                       ReceiverMessageHandler &receiver_handler,
                       SenderMessageHandler &sender_handler) : snap_manager(snap_manager),
                                                               receiver_handler_(receiver_handler),
                                                               sender_handler_(sender_handler) {}

    void sync(DirSnapshot &curr_snap, DirSnapshot &peer_snap, const std::string &peer_snap_version, const std::string &curr_snap_version);

private:
    struct SyncState
    {
        bool peer_snap_present;
        bool peer_snap_version_same;
        std::string fetched_peer_snap_version;
        std::vector<std::string> peer_dir_list;
    };

    // Helper methods for each sync stage
    SyncState fetch_peer_state(const DirSnapshot &curr_snap,
                               DirSnapshot &peer_snap,
                               const std::string &peer_snap_version,
                               const std::string &curr_snap_version);

    void handle_directory_changes(const DirChanges &dir_changes,
                                  const SyncState &state,
                                  DirSnapshot &curr_snap);

    void handle_file_changes(const FileChanges &file_changes,
                             const DirSnapshot &curr_snap,
                             DirSnapshot &peer_snap,
                             const SyncState &state);

    void handle_modified_files(const std::vector<FileModification> &modified_files,
                               const DirSnapshot &curr_snap,
                               const DirSnapshot &peer_snap);

    void finalize_sync(const FileChanges &file_changes, DirSnapshot &curr_snap);
};