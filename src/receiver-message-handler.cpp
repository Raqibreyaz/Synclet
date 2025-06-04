#include "../include/receiver-message-handler.hpp"

/*
  . 'old_chunk_size' will always be used for moving cursor
  . 'chunk_size will be used for writing that amount of data'
  in case of fetching modified chunk:
  - for fetching data 'old_chunk_size' will be used for modify and 'chunk_size' for others
  in case of receiving modified_chunk:
  - data received will be always 'chunk_size'
  */

ReceiverMessageHandler::ReceiverMessageHandler(const std::string &working_dir, Messenger &messenger)
    : working_dir(working_dir),
      messenger(messenger) {}

// creates and appends stream data to file then create and add snapshot
void ReceiverMessageHandler::process_create_file(const FileCreateRemovePayload &payload, DirSnapshot &snaps)
{
    process_get_files(std::vector<std::string>({payload.filename}));

    snaps[payload.filename] = SnapshotManager::createSnapshot(working_dir + "/" + payload.filename);
}

// creates a list of files and appends stream data to them then create and add snapshots
void ReceiverMessageHandler::process_create_file(const FilesCreatedPayload &payload, DirSnapshot &snaps)
{
    process_get_files(payload.files);

    for (auto &filename : payload.files)
        snaps[filename] = SnapshotManager::createSnapshot(working_dir + "/" + filename);
}

// deletes a given file and removes from snaps
void ReceiverMessageHandler::process_delete_file(const FileCreateRemovePayload &payload, DirSnapshot &snaps)
{
    const auto &filepath = working_dir + "/" + payload.filename;

    // before removing check if file exists
    if (!fs::exists(filepath) || !fs::remove(filepath))
        std::cerr << "failed to delete file: " << payload.filename << std::endl;

    snaps.erase(payload.filename);
}

// deletes a given list of files and removes from snaps too!
void ReceiverMessageHandler::process_delete_file(const FilesRemovedPayload &payload, DirSnapshot &snaps)
{
    for (auto &filename : payload.files)
    {
        const auto &filepath = working_dir + "/" + filename;

        // before removing check if file exists
        if (!fs::exists(filepath) || !fs::remove(filepath))
            std::cerr << "failed to delete file: " << filename << std::endl;

        // remove the file from snaps too!
        else
            snaps.erase(filename);
    }
}

// rename the provided file and rename in snaps too!
void ReceiverMessageHandler::process_file_rename(const FileRenamePayload &payload, DirSnapshot &snaps)
{
    const auto &old_filepath = working_dir + "/" + payload.old_filename;
    const auto &new_filepath = working_dir + "/" + payload.new_filename;

    fs::rename(old_filepath, new_filepath);

    auto file_snap = std::move(snaps[payload.old_filename]);
    snaps[payload.new_filename] = std::move(file_snap);
}

// handle the case where peer is sending modified chunks and update the snap of that file too!
void ReceiverMessageHandler::process_modified_chunk(const ModifiedChunkPayload &provided_payload, DirSnapshot &snaps)
{
    ChunkHandler chunk_handler(provided_payload.filename);

    auto payload = provided_payload;

    while (true)
    {
        ChunkMetadata chunk_md{
            .chunk_type = payload.chunk_type,
            .offset = payload.offset,
            .chunk_size = payload.chunk_size,
            .old_chunk_size = payload.old_chunk_size,
            .is_last_chunk = payload.is_last_chunk};

        // now take the chunk data if it is not removed
        const std::string &data = payload.chunk_type != ChunkType::REMOVE ? messenger.receive_max_given_bytes(payload.chunk_size) : "";

        if (data.size() != payload.chunk_size)
            std::clog << std::format("requested {} size data but got {} size", payload.chunk_size, data.size());

        // save the whole chunk to corresponding file
        chunk_handler.save_chunk(chunk_md, data);

        // leave when this was the last chunk
        if (payload.is_last_chunk)
            break;

        const Message &msg = messenger.receive_json_message();

        if (auto payload_ptr = std::get_if<ModifiedChunkPayload>(&(msg.payload)))
            payload = *payload_ptr;

        else
        {
            std::cerr << "invalid payload received for modified chunk";
            break;
        }
    }

    const auto &filepath = working_dir + "/" + payload.filename;

    // now create a final version using the chunks + copying data on gaps
    chunk_handler.finalize_file(filepath);

    // now update the snap of the file
    snaps[payload.filename] = SnapshotManager::createSnapshot(filepath);
}

// just opens the file appends the data and updates the snap of the file
void ReceiverMessageHandler::process_file_chunk(const SendChunkPayload &payload, DirSnapshot &snaps)
{
    FilePairSession file_session(working_dir + "/" + payload.filename, true);
    file_session.ensure_files_open();

    const std::string &chunk_data = messenger.receive_max_given_bytes(payload.chunk_size);
    file_session.append_data(chunk_data);
    file_session.close_session();

    snaps[payload.filename] = SnapshotManager::createSnapshot(working_dir + "/" + payload.filename);
}

// request peer to send the list of files and save on receiving + creates snaps of that files
void ReceiverMessageHandler::process_fetch_files(const std::vector<std::string> &files, DirSnapshot &snaps)
{
    RequestDownloadFilesPayload req_payload;
    req_payload.files = files;

    const Message &msg{
        .type = MessageType::REQ_DOWNLOAD_FILES,
        .payload = std::move(req_payload)};

    messenger.send_json_message(msg);

    // now get all the files and save them
    process_get_files(files);

    // now add the snaps of the provided files
    for (auto &filename : files)
        snaps[filename] = SnapshotManager::createSnapshot(working_dir + "/" + filename);
}

// fetch modified chunks from peer and save in your file
void ReceiverMessageHandler::process_fetch_modified_chunks(const std::vector<FileModification> &modified_files, DirSnapshot &snaps)
{

    // fetches that amount of chunk and saves to corresponding chunk file
    const auto save_as_chunk_file = [&](
                                        ChunkHandler &chunk_handler,
                                        const ChunkType chunk_type,
                                        const uint64_t offset,
                                        const uint64_t chunk_size,
                                        const uint64_t old_chunk_size,
                                        const bool is_last_chunk)
    {
        ChunkMetadata chunk_md{
            .chunk_type = chunk_type,
            .offset = offset,
            .chunk_size = chunk_size,
            .old_chunk_size = old_chunk_size,
            .is_last_chunk = is_last_chunk};

        std::string &&chunk_data = "";

        if (chunk_type != ChunkType::REMOVE)
            chunk_data = messenger.receive_max_given_bytes(chunk_size);

        chunk_handler.save_chunk(chunk_md, chunk_data);
    };

    for (auto &file_modification : modified_files)
    {
        ChunkHandler chunk_handler(file_modification.filename);

        // fetch the new and modified chunks and save chunks
        for (size_t i = 0; i < file_modification.modified_chunks.size(); ++i)
        {
            const auto &modified_chunk = file_modification.modified_chunks[i];

            // added chunk will be saved as removable
            if (modified_chunk.chunk_type == ChunkType::ADD)
            {
                save_as_chunk_file(chunk_handler,
                                   ChunkType::REMOVE,
                                   modified_chunk.offset,
                                   modified_chunk.chunk_size,
                                   0,
                                   modified_chunk.is_last_chunk);
                continue;
            }

            const bool is_modified_chunk = modified_chunk.chunk_type == ChunkType::MODIFY;
            const size_t chunk_size_to_fetch = is_modified_chunk
                                                   ? modified_chunk.old_chunk_size
                                                   : modified_chunk.chunk_size;

            //  in case of add we dont have to move so 0
            const size_t chunk_size_to_move = is_modified_chunk ? modified_chunk.chunk_size : 0;

            RequestChunkPayload req_chunk_payload{
                .filename = file_modification.filename,
                .offset = modified_chunk.offset,
                .chunk_size = chunk_size_to_fetch};

            const Message &msg{
                .type = MessageType::REQ_CHUNK,
                .payload = std::move(req_chunk_payload)};

            // request peer to send this chunk
            messenger.send_json_message(msg);

            // receive that chunk metadata
            const Message &peer_message = messenger.receive_json_message();

            // parse into required type + save the chunk metadata and data
            if (auto payload = std::get_if<SendChunkPayload>(&(peer_message.payload)))
            {
                if (payload->chunk_size != chunk_size_to_fetch)
                    std::cerr << "received inequal chunk on response" << std::endl;

                // removed will be saved as added type
                save_as_chunk_file(chunk_handler,
                                   is_modified_chunk ? ChunkType::MODIFY : ChunkType::ADD,
                                   modified_chunk.offset,
                                   payload->chunk_size,
                                   chunk_size_to_move,
                                   modified_chunk.is_last_chunk);
            }
            else
            {
                std::cerr << "invalid data received from peer" << std::endl;
                return;
            }

            print_progress_bar("fetching file changes...", static_cast<double>(i + 1) / file_modification.modified_chunks.size());
        }
        std::clog << "\n\n";

        const auto &filepath = working_dir + "/" + file_modification.filename;

        // finally create the file
        chunk_handler.finalize_file(filepath);

        // now update the snap of the file
        snaps[file_modification.filename] = SnapshotManager::createSnapshot(filepath);
    }
}

// request and returnt the snapshot version of peer
std::string ReceiverMessageHandler::process_request_snap_version()
{
    Message msg{.type = MessageType::REQ_SNAP_VERSION, .payload = {}};
    messenger.send_json_message(msg);

    // receive snap_version of data from server
    const Message &peer_message = messenger.receive_json_message();

    if (peer_message.type != MessageType::SNAP_VERSION)
        throw std::runtime_error("invalid type of message");

    if (auto payload_ptr = std::get_if<SnapVersionPayload>(&(peer_message.payload)))
        return payload_ptr->snap_version;
    else
        throw std::runtime_error("invalid data received");
}

// request and return the snapshot of peer
void ReceiverMessageHandler::process_request_peer_snap(DirSnapshot &peer_snaps)
{
    const Message msg{.type = MessageType::REQ_SNAP, .payload = {}};

    messenger.send_json_message(msg);

    // receive snap of data from peer
    const Message &peer_message = messenger.receive_json_message();

    if (peer_message.type != MessageType::DATA_SNAP)
        throw std::runtime_error("invalid type of message");

    // now update the peer snaps using the provided snaps of peer
    if (auto payload_ptr = std::get_if<DataSnapshotPayload>(&(peer_message.payload)))
    {
        // removing every entry to store fresh snapshots
        peer_snaps.clear();

        for (auto &file_snap : payload_ptr->files)
            peer_snaps[file_snap.filename] = file_snap;
    }
    else
        throw std::runtime_error("invalid data received");
}

void ReceiverMessageHandler::process_get_files(const std::vector<std::string> &files)
{
    for (size_t i = 0; i < files.size(); i++)
    {
        const std::string &filename = files[i];

        // file will be created on opening it
        FilePairSession file_session(working_dir + "/" + filename, true);
        file_session.ensure_files_open();

        while (true)
        {
            const Message &msg = messenger.receive_json_message();

            if (msg.type != MessageType::SEND_CHUNK)
                std::runtime_error("invalid message type received!");

            // now fetch all the chunks and append to file
            if (auto chunk_payload = std::get_if<SendChunkPayload>(&(msg.payload)))
            {

                if (chunk_payload->filename != filename)
                    throw std::runtime_error(std::format("invalid chunk received from another file: {} instead of {}", chunk_payload->filename, filename));

                const std::string &chunk_data = messenger.receive_max_given_bytes(chunk_payload->chunk_size);

                file_session.append_data(chunk_data);

                if (chunk_payload->is_last_chunk)
                {
                    file_session.close_session();
                    break;
                }
            }
            else
                throw std::runtime_error("invalid payload type received!");
        }
        print_progress_bar("fetching files...", static_cast<double>(i + 1) / files.size());
    }
    std::clog << "\n\n";
}