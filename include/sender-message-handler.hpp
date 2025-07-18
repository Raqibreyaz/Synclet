#pragma once
#include "messenger.hpp"
#include "file-event.hpp"
#include "snapshot-manager.hpp"

class SenderMessageHandler
{

public:
    SenderMessageHandler(const Messenger &messenger, const std::string &working_dir);
    void handle_event(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_changes(const FileChanges &dir_changes) const;

    // private:
    const Messenger &messenger;
    const std::string working_dir;
    void handle_create_file(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_create_file(const std::vector<FileSnapshot> &files) const;
    void handle_delete_file(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_delete_file(const std::vector<std::string> &files) const;
    void handle_moved_file(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_modify_file(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_modify_file(const std::vector<FileModification> &modified_files) const;
    void handle_create_dir(const FileEvent &event)const;
    void handle_create_dir(const std::vector<std::string> &dirs) const;
    void handle_delete_dir(const FileEvent& event,DirSnapshot& curr_snap)const;
    void handle_delete_dir(const std::vector<std::string>& dirs)const;
    void handle_moved_dir(const FileEvent& event,DirSnapshot& curr_snap)const;
    void handle_file_sync(const FileSnapshot &file_snap) const;
    void handle_file_modification_sync(const FileModification &file_modification) const;
    void handle_request_snap_version(const std::string &snap_version);
    void handle_request_snap(DirSnapshot &snapshot);
    void handle_request_chunk(const RequestChunkPayload &payload);
    void handle_request_download_files(const RequestDownloadFilesPayload &payload, DirSnapshot &curr_snap);
    void handle_request_dir_list();
};
