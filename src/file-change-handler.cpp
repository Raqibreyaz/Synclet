#include "../include/file-change-handler.hpp"

FileChangeHandler::FileChangeHandler(const Messenger &messenger, const std::string &working_dir) : messenger(messenger), working_dir(working_dir) {}

// handle file watching events
void FileChangeHandler::handle_event(const FileEvent &event, DirSnapshot &curr_snap) const
{
    switch (event.event_type)
    {
    case EventType::CREATED:
        return handle_create_file(
            curr_snap.at(extract_filename_from_path(event.filepath)));
    case EventType::RENAMED:
        return handle_rename_file(event);
    case EventType::DELETED:
        return handle_delete_file(event);
    case EventType::MODIFIED:
        return handle_modify_file(event, curr_snap);
    default:
        throw std::runtime_error("invalid file event found!!");
    }
}

// inform peer for creation/deletion of file
void FileChangeHandler::handle_create_file(const FileSnapshot &file_snap) const
{
    Message msg;

    msg.type = MessageType::FILE_CREATE;
    msg.payload = FileCreateRemovePayload{.filename = file_snap.filename};

    // inform server to create this file
    messenger.send_json_message(msg);

    // now sync the full file
    handle_file_sync(file_snap);
}

void FileChangeHandler::handle_delete_file(const FileEvent &event) const
{
    Message msg;

    msg.type = MessageType::FILE_REMOVE;
    msg.payload = FileCreateRemovePayload{
        .filename = extract_filename_from_path(event.filepath)};

    messenger.send_json_message(msg);
}

// inform peer for renaming of file
void FileChangeHandler::handle_rename_file(const FileEvent &event) const
{
    // skip the event if it is incomplete
    if (!(event.new_filepath.has_value()) && !(event.old_filepath.has_value()))
        return;

    Message msg;

    // create the message
    msg.type = MessageType::FILE_RENAME;
    msg.payload = FileRenamePayload{
        .old_filename = extract_filename_from_path(event.old_filepath.value().string()),
        .new_filename = extract_filename_from_path(event.new_filepath.value().string())};

    messenger.send_json_message(msg);
}

// inform and sync delta of file
void FileChangeHandler::handle_modify_file(const FileEvent &event, DirSnapshot &snap) const
{
    const std::string &filepath = event.filepath;
    const std::string &&filename = extract_filename_from_path(filepath);

    FileSnapshot &&curr_file_snap = SnapshotManager::createSnapshot(filepath);
    FileSnapshot &prev_file_snap = snap.at(filename);

    // will return changes in sorted order means every array(added,modified,removed all will be sorted)
    FileModification &&file_modification = SnapshotManager::get_file_modification(curr_file_snap, prev_file_snap);

    // now we have to send changes sorted by their offset
    handle_file_modification_sync(file_modification);

    // storing the current snapshot of the file
    snap[filename] = curr_file_snap;
}

// handle file changes
void FileChangeHandler::handle_changes(const DirChanges &dir_changes) const
{
    if (!dir_changes.created_files.empty())
        handle_create_file(dir_changes.created_files);

    if (!dir_changes.removed_files.empty())
        handle_delete_file(dir_changes.removed_files);

    if (!dir_changes.modified_files.empty())
        handle_modify_file(dir_changes.modified_files);
}

// inform peer for creation of files and sync created files
void FileChangeHandler::handle_create_file(const std::vector<FileSnapshot> &files) const
{
    Message msg;

    FilesCreatedPayload payload;
    payload.files = std::vector<std::string>();
    payload.files.reserve(files.size());

    std::transform(files.begin(), files.end(), std::back_inserter(payload.files),
                   [](const auto &file_snap)
                   {
                       return file_snap.filename;
                   });

    msg.type = MessageType::FILES_CREATE;
    msg.payload = std::move(payload);

    // send filenames so that peer will create that files
    messenger.send_json_message(msg);

    // now sync all the files
    for (const auto &file_snap : files)
    {
        handle_file_sync(file_snap);
    }
}

// inform peer about removed files
void FileChangeHandler::handle_delete_file(const std::vector<std::string> &files) const
{
    Message msg;

    msg.type = MessageType::FILES_REMOVE;
    FilesCreatedPayload payload;
    payload.files = files;
    msg.payload = std::move(payload);

    messenger.send_json_message(msg);
}

// inform and sync delta of file
void FileChangeHandler::handle_modify_file(const std::vector<FileModification> &modified_files) const
{
    for (size_t i = 0; i < modified_files.size(); i++)
        handle_file_modification_sync(modified_files[i]);
}

// sync full file
void FileChangeHandler::handle_file_sync(const FileSnapshot &file_snap) const
{
    Message msg;

    const std::string &&filepath = working_dir + "/" + file_snap.filename;

    FileIO fileio(filepath);

    // cant sort hashmap so storing pairs in vector to sort
    std::vector<std::pair<std::string, ChunkInfo>> chunks(file_snap.chunks.begin(), file_snap.chunks.end());

    // sort the chunks
    std::sort(chunks.begin(), chunks.end(), [](const auto &chunk_a, const auto &chunk_b)
              { return chunk_a.second.chunk_no < chunk_b.second.chunk_no; });

    // now send the file chunk by chunk
    for (const auto &[_, chunk] : chunks)
    {
        msg.type = MessageType::SEND_CHUNK;
        msg.payload = SendChunkPayload{
            .filename = file_snap.filename,
            .chunk_size = chunk.chunk_size,
            .chunk_no = chunk.chunk_no};

        // send chunk metadata
        messenger.send_json_message(msg);

        // send chunk data
        messenger.send_file_data(fileio, chunk.offset, chunk.chunk_size);
    }
}

// sync modified part of file
void FileChangeHandler::handle_file_modification_sync(const FileModification &file_modification) const
{
    Message msg;

    const size_t total_modified = file_modification.modified.size();
    const size_t total_added = file_modification.added.size();
    const size_t total_removed = file_modification.removed.size();

    const auto &added_chunks = file_modification.added;
    const auto &modified_chunks = file_modification.modified;
    const auto &removed_chunks = file_modification.removed;

    std::clog << "total_modified: " << total_modified << std::endl;
    std::clog << "total_added: " << total_added << std::endl;
    std::clog << "total_removed: " << total_removed << std::endl;

    const std::string &&filepath = working_dir + "/" + file_modification.filename;

    FileIO fileio(filepath);

    for (size_t i = 0, j = 0, k = 0;
         i < total_added || j < total_removed || k < total_removed;)
    {
        AddRemoveChunkPayload added_chunk;
        AddRemoveChunkPayload removed_chunk;
        ModifiedChunkPayload modified_chunk;

        if (i < added_chunks.size())
            added_chunk = added_chunks[i];
        if (j < removed_chunks.size())
            removed_chunk = removed_chunks[j];
        if (k < modified_chunks.size())
            modified_chunk = modified_chunks[k];

        // check if really using min_offset works
        const size_t min_offset = std::min(added_chunk.offset, std::min(removed_chunk.offset, modified_chunk.offset));

        if (min_offset == added_chunk.offset)
        {
            size_t chunk_size = added_chunk.chunk_size;
            msg.type = MessageType::ADDED_CHUNK;
            msg.payload = std::move(added_chunk);

            messenger.send_json_message(msg);
            messenger.send_file_data(fileio, min_offset, chunk_size);
            ++i;
        }
        else if (min_offset == removed_chunk.offset)
        {
            msg.type = MessageType::REMOVED_CHUNK;
            msg.payload = std::move(removed_chunk);

            messenger.send_json_message(msg);
            ++j;
        }
        else if (min_offset == modified_chunk.offset)
        {
            size_t chunk_size = modified_chunk.chunk_size;
            msg.type = MessageType::MODIFIED_CHUNK;
            msg.payload = std::move(modified_chunk);

            messenger.send_json_message(msg);
            messenger.send_file_data(fileio, min_offset, chunk_size);
            ++k;
        }
    }
}