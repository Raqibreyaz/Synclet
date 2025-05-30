#pragma once
#include "messenger.hpp"
#include "file-event.hpp"
#include "snapshot-manager.hpp"

class FileChangeHandler
{

public:
    FileChangeHandler(const Messenger &messenger, const std::string &working_dir);
    void handle_event(const FileEvent &event, DirSnapshot &curr_snap) const;
    void handle_changes(const DirChanges &dir_changes) const;

private:
    const Messenger &messenger;
    const std::string working_dir;
    void handle_create_file(const FileSnapshot &file_snap) const;
    void handle_delete_file(const FileEvent &event) const;
    void handle_create_file(const std::vector<FileSnapshot> &files) const;
    void handle_delete_file(const std::vector<std::string> &files) const;
    void handle_rename_file(const FileEvent &event) const;
    void handle_modify_file(const FileEvent &event, DirSnapshot &snap) const;
    void handle_modify_file(const std::vector<FileModification> &modified_files) const;
    void handle_file_sync(const FileSnapshot &file_snap) const;
    void handle_file_modification_sync(const FileModification &file_modification) const;
};
