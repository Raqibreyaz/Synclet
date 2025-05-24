#pragma once
#include "messenger.hpp"
#include "file-event.hpp"
#include "snapshot-manager.hpp"

class FileChangeHandler
{

public:
    FileChangeHandler(Messenger &messenger);
    void handle_event(const FileEvent &event, DirSnapshot &prev_snap, DirSnapshot &curr_snap);

private:
    Messenger messenger;
    void handle_create_delete_file(const FileEvent &event);
    void handle_rename_file(const FileEvent &event);
    void handle_modify_file(const FileEvent &event, DirSnapshot &prev_snap, DirSnapshot &curr_snap);
};
