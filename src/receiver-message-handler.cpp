#include "../include/receiver-message-handler.hpp"

ReceiverMessageHandler::ReceiverMessageHandler(const std::string &working_dir, Messenger &messenger)
    : working_dir(working_dir),
      messenger(messenger) {}

// creates and appends stream data to file
void ReceiverMessageHandler::process_create_file(const FileCreateRemovePayload &payload)
{
    process_get_files(std::vector<std::string>({payload.filename}));
}

// creates a list of files and appends stream data to them
void ReceiverMessageHandler::process_create_file(const FilesCreatedPayload &payload)
{
    process_get_files(payload.files);
}

// deletes a given file
void ReceiverMessageHandler::process_delete_file(const FileCreateRemovePayload &payload)
{
    const auto &filepath = working_dir + "/" + payload.filename;

    // before removing check if file exists
    if (!fs::exists(filepath) || !fs::remove(filepath))
        std::cerr << "failed to delete file: " << payload.filename << std::endl;
}

// deletes a given list of files
void ReceiverMessageHandler::process_delete_file(const FilesRemovedPayload &payload)
{
    for (auto &filename : payload.files)
    {
        const auto &filepath = working_dir + "/" + filename;

        // before removing check if file exists
        if (!fs::exists(filepath) || !fs::remove(filepath))
            std::cerr << "failed to delete file: " << filename << std::endl;
    }
}

// rename the provided file
void ReceiverMessageHandler::process_file_rename(const FileRenamePayload &payload)
{
    const auto &old_filepath = working_dir + "/" + payload.old_filename;
    const auto &new_filepath = working_dir + "/" + payload.new_filename;

    fs::rename(old_filepath, new_filepath);
}

// handle the case where peer is sending modified chunks
void ReceiverMessageHandler::process_modified_chunk(const ModifiedChunkPayload &provided_payload)
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

    // now create a final version using the chunks + copying data on gaps
    chunk_handler.finalize_file(working_dir + "/" + payload.filename);
}

// just opens the file appends the data and closes
void ReceiverMessageHandler::process_file_chunk(const SendChunkPayload &payload)
{
    FilePairSession file_session(working_dir + "/" + payload.filename, true);
    file_session.ensure_files_open();

    const std::string &chunk_data = messenger.receive_max_given_bytes(payload.chunk_size);
    file_session.append_data(chunk_data);
    file_session.close_session();
}

// request peer to send the list of files and save on receiving
void ReceiverMessageHandler::process_fetch_files(const std::vector<std::string> &files)
{
    RequestDownloadFilesPayload req_payload;
    req_payload.files = files;

    const Message &msg{
        .type = MessageType::REQ_DOWNLOAD_FILES,
        .payload = std::move(req_payload)};

    messenger.send_json_message(msg);

    // now get all the files and save them
    process_get_files(files);
}

// fetch modified chunks from peer and save in your file
void ReceiverMessageHandler::process_fetch_modified_chunks(const std::vector<FileModification> &modified_files)
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

        const size_t single_chunk_size = chunk_type == ChunkType::MODIFY ? old_chunk_size : chunk_size;

        std::string &&chunk_data = "";

        if (chunk_type != ChunkType::REMOVE)
            chunk_data = messenger.receive_max_given_bytes(single_chunk_size);

        chunk_handler.save_chunk(chunk_md, chunk_data);
    };

    for (const auto &file_modification : modified_files)
    {
        ChunkHandler chunk_handler(file_modification.filename);

        // fetch the new and modified chunks and save chunks
        for (auto &modified_chunk : file_modification.modified_chunks)
        {
            const bool is_modified_chunk = modified_chunk.chunk_type == ChunkType::MODIFY;

            auto chunk_size = is_modified_chunk ? modified_chunk.old_chunk_size : modified_chunk.chunk_size;

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

            RequestChunkPayload req_chunk_payload{
                .filename = file_modification.filename,
                .offset = modified_chunk.offset,
                .chunk_size = chunk_size};

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
                if (payload->chunk_size != chunk_size)
                    std::cerr << "received inequal chunk on response" << std::endl;

                // removed will be saved as added type
                save_as_chunk_file(chunk_handler,
                                   is_modified_chunk ? ChunkType::MODIFY : ChunkType::ADD,
                                   modified_chunk.offset,
                                   payload->chunk_size,
                                   is_modified_chunk ? modified_chunk.old_chunk_size : 0,
                                   modified_chunk.is_last_chunk);
            }
            else
            {
                std::cerr << "invalid data received from peer" << std::endl;
                return;
            }
        }

        // finally create the file
        chunk_handler.finalize_file(working_dir + "/" + file_modification.filename);
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
std::vector<FileSnapshot> ReceiverMessageHandler::process_request_peer_snap()
{
    const Message msg{.type = MessageType::REQ_SNAP, .payload = {}};

    messenger.send_json_message(msg);

    // receive snap of data from peer
    const Message &peer_message = messenger.receive_json_message();

    if (peer_message.type != MessageType::DATA_SNAP)
        throw std::runtime_error("invalid type of message");

    if (auto payload_ptr = std::get_if<DataSnapshotPayload>(&(peer_message.payload)))
        return payload_ptr->files;
    else
        throw std::runtime_error("invalid data received");
}

void ReceiverMessageHandler::process_get_files(const std::vector<std::string> &files)
{
    for (const auto &filename : files)
    {
        // file will be created on opening it
        FilePairSession file_session(working_dir + "/" + filename, true);
        file_session.ensure_files_open();

        const Message &msg = messenger.receive_json_message();

        if (msg.type != MessageType::SEND_CHUNK)
            std::runtime_error("invalid message type received!");

        // now fetch all the chunks and append to file
        while (auto chunk_payload = std::get_if<SendChunkPayload>(&(msg.payload)))
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
    }
}