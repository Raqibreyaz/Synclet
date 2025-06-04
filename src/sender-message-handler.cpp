#include "../include/sender-message-handler.hpp"

SenderMessageHandler::SenderMessageHandler(const Messenger &messenger, const std::string &working_dir) : messenger(messenger), working_dir(working_dir) {}

// handle file watching events
void SenderMessageHandler::handle_event(const FileEvent &event, DirSnapshot &curr_snap) const
{
    switch (event.event_type)
    {
    case EventType::CREATED:
        return handle_create_file(event, curr_snap);
    case EventType::RENAMED:
        return handle_rename_file(event, curr_snap);
    case EventType::DELETED:
        return handle_delete_file(event, curr_snap);
    case EventType::MODIFIED:
        return handle_modify_file(event, curr_snap);
    default:
        throw std::runtime_error("invalid file event found!!");
    }
}

// inform peer for creation/deletion of file
void SenderMessageHandler::handle_create_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    const std::string &filename = extract_filename_from_path(event.filepath);

    // create a snap of the new file
    curr_snap[filename] = SnapshotManager::createSnapshot(event.filepath);

    handle_create_file(std::vector<FileSnapshot>({curr_snap[filename]}));
}

void SenderMessageHandler::handle_delete_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    const std::string &filename = extract_filename_from_path(event.filepath);

    // remove the entry from map
    curr_snap.erase(filename);

    handle_delete_file(std::vector<std::string>({filename}));
}

// inform peer for renaming of file
void SenderMessageHandler::handle_rename_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    // skip the event if it is incomplete
    if (!(event.new_filepath.has_value()) && !(event.old_filepath.has_value()))
        return;

    Message msg;

    const std::string &old_filename = extract_filename_from_path(event.old_filepath.value().string());
    const std::string &new_filename = extract_filename_from_path(event.new_filepath.value().string());

    FileSnapshot file_snap = std::move(curr_snap[old_filename]);
    curr_snap[new_filename] = std::move(file_snap);

    // create the message
    msg.type = MessageType::FILE_RENAME;
    msg.payload = FileRenamePayload{
        .old_filename = old_filename,
        .new_filename = new_filename};

    messenger.send_json_message(msg);
}

// inform and sync delta of file
void SenderMessageHandler::handle_modify_file(const FileEvent &event, DirSnapshot &snap) const
{
    const std::string &filepath = event.filepath;
    const std::string &filename = extract_filename_from_path(filepath);

    FileSnapshot &&curr_file_snap = SnapshotManager::createSnapshot(filepath);
    FileSnapshot &prev_file_snap = snap.at(filename);

    // storing the current snapshot of the file
    snap[filename] = curr_file_snap;

    // will return changes in sorted order means every array(added,modified,removed all will be sorted)
    const FileModification &file_modification = SnapshotManager::get_file_modification(curr_file_snap, prev_file_snap);

    // now we have to send changes
    handle_file_modification_sync(file_modification);
}

// handle file changes
void SenderMessageHandler::handle_changes(const DirChanges &dir_changes) const
{
    if (!dir_changes.created_files.empty())
        handle_create_file(dir_changes.created_files);

    if (!dir_changes.removed_files.empty())
        handle_delete_file(dir_changes.removed_files);

    if (!dir_changes.modified_files.empty())
        handle_modify_file(dir_changes.modified_files);
}

// inform peer for creation of files and sync created files
void SenderMessageHandler::handle_create_file(const std::vector<FileSnapshot> &files) const
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
    for (size_t i = 0; i < files.size(); i++)
    {
        handle_file_sync(files[i]);
    }
}

// inform peer about removed files
void SenderMessageHandler::handle_delete_file(const std::vector<std::string> &files) const
{
    Message msg;

    FilesCreatedPayload payload;
    payload.files = files;

    msg.type = MessageType::FILES_REMOVE;
    msg.payload = std::move(payload);

    messenger.send_json_message(msg);
}

// inform and sync delta of file
void SenderMessageHandler::handle_modify_file(const std::vector<FileModification> &modified_files) const
{
    for (size_t i = 0; i < modified_files.size(); i++)
    {
        handle_file_modification_sync(modified_files[i]);
    }
}

// sync full file
void SenderMessageHandler::handle_file_sync(const FileSnapshot &file_snap) const
{
    Message msg;

    const std::string &filepath = working_dir + "/" + file_snap.filename;

    FileIO fileio(filepath);

    // cant sort hashmap so storing pairs in vector to sort
    std::vector<std::pair<std::string, ChunkInfo>> chunks(file_snap.chunks.begin(), file_snap.chunks.end());

    // sort the chunks
    std::sort(chunks.begin(), chunks.end(), [](const auto &chunk_a, const auto &chunk_b)
              { return chunk_a.second.chunk_no < chunk_b.second.chunk_no; });

    // now send the file chunk by chunk
    for (size_t i = 0; i < chunks.size(); i++)
    {
        msg.type = MessageType::SEND_CHUNK;
        msg.payload = SendChunkPayload{
            .filename = file_snap.filename,
            .chunk_size = chunks[i].second.chunk_size,
            .chunk_no = chunks[i].second.chunk_no,
            .is_last_chunk = (i == chunks.size() - 1)};

        // send chunk metadata
        messenger.send_json_message(msg);

        // send chunk data
        messenger.send_file_data(fileio, chunks[i].second.offset, chunks[i].second.chunk_size);

        print_progress_bar(std::format("sending {}...", file_snap.filename), static_cast<double>(i + 1) / chunks.size());
    }
    std::clog << "\n\n";
}

// sync modified part of file
void SenderMessageHandler::handle_file_modification_sync(const FileModification &file_modification) const
{
    Message msg;

    const std::string &filepath = working_dir + "/" + file_modification.filename;

    FileIO fileio(filepath);

    for (size_t i = 0; i < file_modification.modified_chunks.size(); i++)
    {
        const auto &modified_chunk = file_modification.modified_chunks[i];

        msg.type = MessageType::MODIFIED_CHUNK;
        msg.payload = modified_chunk;

        messenger.send_json_message(msg);

        // no need to send data in remove chunk case
        if (modified_chunk.chunk_type != ChunkType::REMOVE)
            messenger.send_file_data(fileio, modified_chunk.offset, modified_chunk.chunk_size);

        print_progress_bar(std::format("sending {} changes...", modified_chunk.filename), static_cast<double>(i + 1) / file_modification.modified_chunks.size());
    }
    std::clog << "\n\n";
}

// send the snapshot version to peer
void SenderMessageHandler::handle_request_snap_version(const std::string &snap_version)
{
    Message msg;
    msg.type = MessageType::SNAP_VERSION;
    msg.payload = SnapVersionPayload{.snap_version = snap_version};

    messenger.send_json_message(msg);
}

// send the snapshot to peer in a vector
void SenderMessageHandler::handle_request_snap(DirSnapshot &snapshot)
{
    Message msg;

    std::vector<FileSnapshot> snaps;
    snaps.reserve(snapshot.size());

    // insert all the snaps in the vector
    std::transform(snapshot.begin(),
                   snapshot.end(),
                   std::back_inserter(snaps),
                   [](const auto &snap_pair)
                   {
                       return snap_pair.second;
                   });

    msg.type = MessageType::DATA_SNAP;
    msg.payload = DataSnapshotPayload{.files = std::move(snaps)};

    messenger.send_json_message(msg);
}

// send the requested chunk to peer
void SenderMessageHandler::handle_request_chunk(const RequestChunkPayload &payload)
{
    FileIO fileio(working_dir + "/" + payload.filename);

    const std::string &chunk_data = fileio.read_file_from_offset(payload.offset, payload.chunk_size);

    const Message &msg{.type = MessageType::SEND_CHUNK,
                       .payload = SendChunkPayload{
                           .filename = payload.filename,
                           .chunk_size = chunk_data.size(),
                           .chunk_no = 0,
                           .is_last_chunk = true,
                       }};

    messenger.send_json_message(msg);

    messenger.send_file_data(fileio, payload.offset, payload.chunk_size);
}

// send the requested files to peer
void SenderMessageHandler::handle_request_download_files(const RequestDownloadFilesPayload &payload, DirSnapshot &curr_snap)
{
    // we have filenames only
    // we want chunk_size, chunk_no, is_last_chunk
    // and these will be present in the snap
    for (size_t i = 0; i < payload.files.size(); i++)
    {
        handle_file_sync(curr_snap[payload.files[i]]);
    }
}