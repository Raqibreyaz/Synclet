#include "../include/initial-sync-handler.hpp"
#include <iostream>

void InitialSyncHandler::sync(DirSnapshot &curr_snap, DirSnapshot &peer_snap,
                              std::string &peer_snap_version, std::string &curr_snap_version)
{
    // Stage 1: Fetch peer state
    SyncState state = fetch_peer_state(curr_snap, peer_snap, peer_snap_version, curr_snap_version);

    if (state.peer_snap_version_same)
    {
        std::clog << "No initial changes found\n";
        return;
    }

    // compare both snapshots and find changes
    FileChanges file_changes = snap_manager_.compare_snapshots(curr_snap, peer_snap);

    // get dir changes like new/deleted dirs names
    DirChanges dir_changes = snap_manager_.compare_directories(state.peer_dir_list);

    if (file_changes.created_files.empty() &&
        file_changes.modified_files.empty() &&
        file_changes.removed_files.empty() &&
        dir_changes.added_dirs.empty() &&
        dir_changes.removed_dirs.empty())
        std::clog << "no initial changes found" << std::endl;

    // Stage 3: Handle each type of change
    handle_directory_changes(dir_changes, state, curr_snap);
    handle_file_changes(file_changes, curr_snap, peer_snap, state);

    // Stage 4: Finalize
    finalize_sync(file_changes, curr_snap);

    // now snap version got matched after syncing
    peer_snap_version = state.fetched_peer_snap_version;
}

InitialSyncHandler::SyncState InitialSyncHandler::fetch_peer_state(
    const DirSnapshot &curr_snap,
    DirSnapshot &peer_snap,
    std::string &peer_snap_version,
    const std::string &curr_snap_version)
{

    SyncState state;
    state.peer_snap_present = !peer_snap.empty();
    state.fetched_peer_snap_version = receiver_handler_.process_request_snap_version();
    state.peer_snap_version_same = state.peer_snap_present &&
                                   state.fetched_peer_snap_version == peer_snap_version;

    // when peer is up to date then go back
    if (state.fetched_peer_snap_version == curr_snap_version)
    {
        if (!state.peer_snap_present || !state.peer_snap_version_same)
        {
            peer_snap = curr_snap;
            peer_snap_version = curr_snap_version;
            snap_manager_.save_snapshot(curr_snap);
            state.peer_snap_version_same = true;
        }
        return state;
    }

    // Fetch peer snapshot if outdated or missing
    if (!state.peer_snap_present || !state.peer_snap_version_same)
    {
        receiver_handler_.process_request_peer_snap(peer_snap);
        receiver_handler_.process_request_peer_dir_list(state.peer_dir_list);
    }

    return state;
}

void InitialSyncHandler::handle_directory_changes(const DirChanges &dir_changes,
                                                  const SyncState &state,
                                                  DirSnapshot &curr_snap)
{

    // Handle added directories
    if (!dir_changes.added_dirs.empty())
    {
        // delete the directories if last sync to peer was not latest
        // if (!state.peer_snap_present || !state.peer_snap_version_same)
        // {
        //     // delete the directories if last sync to peer was not latest
        //     receiver_handler_.process_delete_dir(
        //         DirsCreatedRemovedPayload{.dirs = dir_changes.added_dirs},
        //         curr_snap);
        // }
        // else
        // {
        sender_handler_.handle_create_dir(dir_changes.added_dirs);
        // }
    }

    // Handle removed directories
    if (!dir_changes.removed_dirs.empty())
    {
        // add the directories if last sync was not latest
        if (!state.peer_snap_present || !state.peer_snap_version_same)
        {
            receiver_handler_.process_create_dir(
                DirsCreatedRemovedPayload{.dirs = dir_changes.removed_dirs});
        }
        else
        {
            sender_handler_.handle_delete_dir(dir_changes.removed_dirs);
        }
    }
}

void InitialSyncHandler::handle_file_changes(const FileChanges &file_changes,
                                             DirSnapshot &curr_snap,
                                             DirSnapshot &peer_snap,
                                             const SyncState &state)
{
    // Handle created files
    if (!file_changes.created_files.empty())
    {
        std::clog << "Syncing new files to peer...\n";
        sender_handler_.handle_create_file(file_changes.created_files);
    }

    // Handle removed files
    if (!file_changes.removed_files.empty())
    {
        // fetch all the deleted files from peer if it's snap wasn't present or new snap available
        // + update the curr_snap
        if (!state.peer_snap_present || !state.peer_snap_version_same)
        {
            std::clog << "Downloading new files from peer...\n";
            receiver_handler_.process_fetch_files(file_changes.removed_files, curr_snap);
        }

        // when curr snap was up to date then ask server to delete the files
        else
        {
            std::clog << "Requesting peer to delete files\n";
            sender_handler_.handle_delete_file(file_changes.removed_files);
        }
    }

    // Handle modified files
    if (!file_changes.modified_files.empty())
    {
        handle_modified_files(file_changes.modified_files, curr_snap, peer_snap);
    }
}

void InitialSyncHandler::handle_modified_files(
    const std::vector<FileModification> &modified_files,
    DirSnapshot &curr_snap,
    const DirSnapshot &peer_snap)
{

    std::vector<FileModification> to_fetch;
    std::vector<FileModification> to_change;

    for (const auto &file : modified_files)
    {
        // Peer file is newer: fetch from peer
        if (curr_snap.at(file.filename).mtime < peer_snap.at(file.filename).mtime)
        {
            to_fetch.push_back(file);
        }
        // Current file is newer: sync to peer
        else
        {
            to_change.push_back(file);
        }
    }

    if (!to_change.empty())
    {
        std::clog << "Syncing file changes to peer\n";
        sender_handler_.handle_modify_file(to_change);
    }

    if (!to_fetch.empty())
    {
        std::clog << "Fetching modified files from peer\n";
        receiver_handler_.process_fetch_modified_chunks(to_fetch, curr_snap);
    }
}

void InitialSyncHandler::finalize_sync(const FileChanges &file_changes,
                                       DirSnapshot &curr_snap)
{

    if (!file_changes.created_files.empty() ||
        !file_changes.modified_files.empty() ||
        !file_changes.removed_files.empty())
    {

        std::clog << "Saving peer snap as cache\n";
        snap_manager_.save_snapshot(curr_snap);
    }
}
