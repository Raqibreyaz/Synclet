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

    FileModification file_modification = SnapshotManager::get_file_modification(curr_file_snap, prev_file_snap);

    // open the file for IO
    FileIO fileio(filepath);

    // removed chunks
    for (TruncateFilePayload &removed_chunk : file_modification.removed)
    {
        msg.type = MessageType::REMOVED_CHUNK;
        msg.payload = std::move(removed_chunk);

        messenger.send_json_message(msg);
    }

    // modified chunks
    for (ModifiedChunkPayload &modified_chunk : file_modification.modified)
    {
        size_t offset = modified_chunk.new_start_index;
        size_t chunk_size = modified_chunk.old_end_index - offset + 1;

        msg.type = MessageType::MODIFIED_CHUNK;
        msg.payload = std::move(modified_chunk);

        messenger.send_json_message(msg);
        messenger.send_file_data(fileio, offset, chunk_size);
    }

    // new chunks
    for (AddChunkPayload &added_chunk : file_modification.added)
    {
        size_t offset = added_chunk.new_start_index;
        size_t chunk_size = added_chunk.new_end_index - offset + 1;

        msg.type = MessageType::ADDED_CHUNK;
        msg.payload = std::move(added_chunk);

        messenger.send_json_message(msg);
        messenger.send_file_data(fileio, offset, chunk_size);
    }
}