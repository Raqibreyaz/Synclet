#include "../include/file-change-handler.hpp"

FileChangeHandler::FileChangeHandler(Messenger &messenger) : messenger(messenger) {}

void FileChangeHandler::handle_event(const FileEvent &event, DirSnapshot &prev_snap, DirSnapshot &curr_snap)
{
    switch (event.event_type)
    {
    case EventType::CREATED:
        handle_create_delete_file(event);
        break;
    case EventType::RENAMED:
        handle_rename_file(event);
        break;
    case EventType::DELETED:
        handle_create_delete_file(event);
        break;
    case EventType::MODIFIED:
        handle_modify_file(event, prev_snap, curr_snap);
        break;
    default:
        throw std::runtime_error("invalid file event found!!");
    }
}

void FileChangeHandler::handle_create_delete_file(const FileEvent &event)
{
    Message msg;

    msg.type = event.event_type == EventType::CREATED ? MessageType::FILE_CREATE : MessageType::FILE_REMOVE;
    msg.payload = FileCreateRemovePayload{
        .filename = extract_filename_from_path(event.filepath)};

    messenger.send_json_message(msg);
}

void FileChangeHandler::handle_rename_file(const FileEvent &event)
{
    // skip the event if it is incomplete
    if (!(event.new_filepath.has_value()) && !(event.old_filepath.has_value()))
        return;

    Message msg;

    // create the message
    msg.type = MessageType::FILE_RENAME;
    msg.payload = FileRenamePayload{
        .old_filename = event.new_filepath.value().string(),
        .new_filename = event.old_filepath.value().string()};

    messenger.send_json_message(msg);
}
void FileChangeHandler::handle_modify_file(const FileEvent &event, DirSnapshot &prev_snap, DirSnapshot &curr_snap)
{
    Message msg;
    const std::string filepath = event.filepath;
    const std::string filename = extract_filename_from_path(filepath);

    FileSnapshot curr_file_snap = SnapshotManager::createSnapshot(event.filepath);

    FileSnapshot prev_file_snap = prev_snap[filename];

    // will return changes in sorted order means every array(added,modified,removed all will be sorted)
    FileModification file_modification = SnapshotManager::get_file_modification(curr_file_snap, prev_file_snap);

    // open the file for IO
    FileIO fileio(filepath);

    size_t total_modified = file_modification.modified.size();
    size_t total_added = file_modification.added.size();
    size_t total_removed = file_modification.removed.size();

    // now we have to send changes sorted by their offset
    for (size_t i = 0, j = 0, k = 0;
         i < total_added || j < total_removed || k < total_modified;)
    {
        AddRemoveChunkPayload added_chunk;
        AddRemoveChunkPayload removed_chunk;
        ModifiedChunkPayload modified_chunk;

        if (i < total_added)
            added_chunk = file_modification.added[i];
        if (j < total_removed)
            removed_chunk = file_modification.removed[j];
        if (k < total_modified)
            modified_chunk = file_modification.modified[k];

            // check if really using min_offset works
        size_t min_offset = std::min(added_chunk.offset, std::min(removed_chunk.offset, modified_chunk.offset));

        if (min_offset == added_chunk.offset)
        {
            size_t chunk_size = added_chunk.chunk_size;
            msg.type = MessageType::ADDED_CHUNK;
            msg.payload = std::move(added_chunk);

            messenger.send_json_message(msg);
            messenger.send_file_data(fileio, min_offset, chunk_size);
            ++i;
        }
        if (min_offset == removed_chunk.offset)
        {
            msg.type = MessageType::REMOVED_CHUNK;
            msg.payload = std::move(removed_chunk);

            messenger.send_json_message(msg);
            ++j;
        }
        if (min_offset == modified_chunk.offset)
        {
            size_t chunk_size = added_chunk.chunk_size;
            msg.type = MessageType::MODIFIED_CHUNK;
            msg.payload = std::move(modified_chunk);

            messenger.send_json_message(msg);
            messenger.send_file_data(fileio, min_offset, chunk_size);
            ++k;
        }
    }
}