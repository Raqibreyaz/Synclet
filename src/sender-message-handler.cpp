#include "../include/sender-message-handler.hpp"

SenderMessageHandler::SenderMessageHandler(const Messenger &messenger, const std::string &working_dir) : messenger(messenger), working_dir(working_dir) {}

// handle file watching events
void SenderMessageHandler::handle_event(const FileEvent &event, DirSnapshot &curr_snap) const
{
    switch (event.event_type)
    {
    case EventType::CREATED:
        return event.is_directory
                   ? handle_create_dir(event)
                   : handle_create_file(event, curr_snap);
    case EventType::MOVED:
        return event.is_directory
                   ? handle_moved_dir(event, curr_snap)
                   : handle_moved_file(event, curr_snap);
    case EventType::DELETED:
        return event.is_directory
                   ? handle_delete_dir(event, curr_snap)
                   : handle_delete_file(event, curr_snap);
    case EventType::MODIFIED:
        return handle_modify_file(event, curr_snap);
    default:
        throw std::runtime_error("invalid file event found!!");
    }
}

// inform peer for creation/deletion of file
void SenderMessageHandler::handle_create_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    const std::string fullpath = std::format("{}/{}", working_dir, event.filepath.string());

    // create an empty snap of the new file
    curr_snap[event.filepath] = FileSnapshot(event.filepath, 0,
                                             to_unix_timestamp(fs::last_write_time(fullpath)),
                                             {});

    // send filename so that peer can create it
    Message msg{
        .type = MessageType::FILE_CREATE,
        .payload = FileCreateRemovePayload{.filename = event.filepath},
    };
    messenger.send_json_message(msg);
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
        if (files[i].chunks.empty())
            continue;

        handle_file_sync(files[i]);
    }
}

// inform peer about a single removed file
void SenderMessageHandler::handle_delete_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    // remove the entry from map
    curr_snap.erase(event.filepath);

    Message msg{
        .type = MessageType::FILE_REMOVE,
        .payload = FileCreateRemovePayload{.filename = event.filepath},
    };

    messenger.send_json_message(msg);
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

// inform peer for renaming of file
void SenderMessageHandler::handle_moved_file(const FileEvent &event, DirSnapshot &curr_snap) const
{
    // skip the event if it is incomplete
    if (!(event.new_filepath.has_value()) || !(event.old_filepath.has_value()))
        return;

    Message msg;

    FileSnapshot file_snap = std::move(curr_snap[event.old_filepath.value().string()]);
    curr_snap[event.new_filepath.value().string()] = std::move(file_snap);
    curr_snap.erase(event.old_filepath.value().string());

    // create the message
    msg.type = MessageType::FILE_MOVED;
    msg.payload = FileMovedPayload{
        .old_filename = event.old_filepath.value().string(),
        .new_filename = event.new_filepath.value().string()};

    messenger.send_json_message(msg);
}

// inform and sync delta of file
void SenderMessageHandler::handle_modify_file(const FileEvent &event, DirSnapshot &snap) const
{
    const std::string &full_path = std::format("{}/{}", working_dir, event.filepath.string());

    FileSnapshot &prev_file_snap = snap.at(event.filepath.string());
    FileSnapshot &&curr_file_snap = SnapshotManager::createSnapshot(full_path, working_dir);

    // will return changes in sorted order means every array(added,modified,removed all will be sorted)
    const FileModification &file_modification = SnapshotManager::get_file_modification(curr_file_snap, prev_file_snap);

    // now we have to send changes
    handle_file_modification_sync(file_modification);

    // storing the current snapshot of the file
    snap[event.filepath] = curr_file_snap;
}

// inform peer to create the dir
void SenderMessageHandler::handle_create_dir(const FileEvent &event) const
{
    Message msg{
        .type = MessageType::DIR_CREATE,
        .payload = DirCreateRemovePayload{
            .dir_path = event.filepath.string()}};

    messenger.send_json_message(msg);
}

// inform peer to create these dirs
void SenderMessageHandler::handle_create_dir(const std::vector<std::string> &dirs) const
{

    Message msg{
        .type = MessageType::DIRS_CREATE,
        .payload = DirsCreatedRemovedPayload{
            .dirs = dirs}};

    messenger.send_json_message(msg);
}

void SenderMessageHandler::handle_delete_dir(const FileEvent &event, DirSnapshot &curr_snap) const
{

    Message msg{
        .type = MessageType::DIR_REMOVE,
        .payload = DirCreateRemovePayload{.dir_path = event.filepath}};

    messenger.send_json_message(msg);

    // remove entry from snaps with this format
    std::string a = std::format("{}/", event.filepath.string());
    std::string b = std::format("./{}/", event.filepath.string());
    std::string c = std::format("/{}/", event.filepath.string());

    // remove entries from the snap of this folder
    for (auto it = curr_snap.begin(); it != curr_snap.end();)
    {
        // remove the file entry from snaps
        if (
            it->first.starts_with(a) ||
            it->first.find(b) != std::string::npos ||
            it->first.find(c) != std::string::npos)
            it = curr_snap.erase(it);
        else
            it++;
    }
}

void SenderMessageHandler::handle_delete_dir(const std::vector<std::string> &dirs) const
{
    Message msg{
        .type = MessageType::DIRS_REMOVE,
        .payload = DirsCreatedRemovedPayload{
            .dirs = dirs}};

    messenger.send_json_message(msg);
}

void SenderMessageHandler::handle_moved_dir(const FileEvent &event, DirSnapshot &curr_snap) const
{
    // skip the event if no old or new path provided
    if (!event.old_filepath.has_value() || !event.new_filepath.has_value())
        return;

    // inform peer to move the directory at its side too!
    Message msg{
        .type = MessageType::DIR_MOVED,
        .payload = DirMovedPayload{
            .old_dir_path = event.old_filepath.value().string(),
            .new_dir_path = event.new_filepath.value().string()}};
    messenger.send_json_message(msg);

    // updating the new path in each file entry
    std::vector<std::pair<std::string, FileSnapshot>> snap_arr;
    std::string old_dir_path = event.old_filepath.value().string();
    for (auto it = curr_snap.begin(); it != curr_snap.end();)
    {

        // finding position where the dir starts
        size_t dir_start = it->first.find(old_dir_path);

        // skip if the directory is not present in the path
        if (dir_start == std::string::npos)
        {
            it++;
            continue;
        }

        // create new path
        std::string new_dir_path =
            it->first.substr(0, dir_start) +
            event.new_filepath.value().string() +
            it->first.substr(dir_start + old_dir_path.size());

        // add new path and the snap to vector
        snap_arr.push_back({new_dir_path, std::move(it->second)});

        // remove the entry from map
        it = curr_snap.erase(it);
    }

    // moving the entries in map
    for (auto &p : snap_arr)
        curr_snap.insert(std::move(p));
}

// handle file changes
void SenderMessageHandler::handle_changes(const FileChanges &file_changes) const
{
    if (!file_changes.created_files.empty())
        handle_create_file(file_changes.created_files);

    if (!file_changes.removed_files.empty())
        handle_delete_file(file_changes.removed_files);

    if (!file_changes.modified_files.empty())
        handle_modify_file(file_changes.modified_files);
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

    // send the file metadata
    msg.type = MessageType::SEND_FILE;
    msg.payload = SendFilePayload{
        .filename = file_snap.filename,
        .file_size = file_snap.file_size,
        .no_of_chunks = static_cast<int>(file_snap.chunks.size()),
    };
    messenger.send_json_message(msg);

    // now sending chunks

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

void SenderMessageHandler::handle_request_dir_list()
{
    // get all the directories from the data dir
    std::vector<std::string> dirs;
    for (auto &entry : fs::recursive_directory_iterator(working_dir))
    {
        if (entry.is_directory())
        {
            const std::string &dir_path = extract_filename_from_path(working_dir, entry.path().string());
            dirs.push_back(dir_path);
        }
    }

    // now send all the directories to peer
    DirListPayload dir_list_payload{.dirs = std::move(dirs)};
    const Message msg{
        .type = MessageType::DIR_LIST,
        .payload = std::move(dir_list_payload)};

    messenger.send_json_message(msg);
}