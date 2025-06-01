#include "../include/server-message-handler.hpp"

ServerMessageHandler::ServerMessageHandler(const std::string &working_dir, TcpConnection &client, Messenger &messenger)
    : working_dir(working_dir),
      client(client),
      messenger(messenger) {}

// returns the session of that particular file, creates one if not present
void ServerMessageHandler::get_or_create_session(const std::string &filename, const bool append_to_original)
{
    const std::string filepath = working_dir + "/" + filename;

    if (!active_session)
        active_session = std::make_unique<FilePairSession>(filepath, append_to_original);

    active_session->reset_if_filepath_changes_append_required(filepath, append_to_original);
    active_session->ensure_files_open();
}

void ServerMessageHandler::process_create_file(const FileCreateRemovePayload &payload)
{
    // file will be created on opening it
    FileIO fileio(working_dir + "/" + payload.filename, std::ios::out | std::ios::app);

    Message msg = messenger.receive_json_message();

    if (msg.type != MessageType::SEND_CHUNK)
        throw std::runtime_error("invalid message type received!");

    // now fetch all the chunks and append to file
    while (auto chunk_payload = std::get_if<SendChunkPayload>(&(msg.payload)))
    {
        if (chunk_payload->filename != payload.filename)
            throw std::runtime_error(std::format("invalid chunk received from another file: {} instead of {}", chunk_payload->filename, payload.filename));

        std::string chunk_data = client.receiveSome(chunk_payload->chunk_size);

        fileio.append_chunk(chunk_data);

        if (chunk_payload->is_last_chunk)
            return;
    }
}

void ServerMessageHandler::process_create_file(const FilesCreatedPayload &payload)
{
    for (const auto &filename : payload.files)
    {
        // file will be created on opening it
        FilePairSession file_session(working_dir + "/" + filename, true);

        Message msg = messenger.receive_json_message();

        if (msg.type != MessageType::SEND_CHUNK)
            std::runtime_error("invalid message type received!");

        // now fetch all the chunks and append to file
        while (auto chunk_payload = std::get_if<SendChunkPayload>(&(msg.payload)))
        {
            if (chunk_payload->filename != filename)
                throw std::runtime_error(std::format("invalid chunk received from another file: {} instead of {}", chunk_payload->filename, filename));

            std::string chunk_data = client.receiveSome(chunk_payload->chunk_size);
            file_session.append_data(chunk_data);

            if (chunk_payload->is_last_chunk)
                break;
        }
    }
}

void ServerMessageHandler::process_delete_file(const FileCreateRemovePayload &payload)
{
    auto filepath = working_dir + "/" + payload.filename;

    // before removing check if file exists
    if (fs::exists(filepath))
        fs::remove(filepath);
    else
        std::cerr << "given file already not exists!";
}

void ServerMessageHandler::process_delete_file(const FilesRemovedPayload &payload)
{
    for (auto &filename : payload.files)
    {
        auto filepath = working_dir + "/" + filename;

        // before removing check if file exists
        if (fs::exists(filepath))
            fs::remove(filepath);
        else
            std::cerr << "given file already not exists!";
    }
}

void ServerMessageHandler::process_file_rename(const FileRenamePayload &payload)
{
    auto old_filepath = working_dir + "/" + payload.old_filename;
    auto new_filepath = working_dir + "/" + payload.new_filename;

    fs::rename(old_filepath, new_filepath);
}

void ServerMessageHandler::process_modified_chunk(const ModifiedChunkPayload &payload)
{
    ChunkHandler chunk_handler(payload.filename);

    ChunkMetadata chunk_md{
        .op_type = OP_TYPE::MODIFY,
        .offset = payload.offset,
        .chunk_size = payload.chunk_size,
        .old_chunk_size = payload.old_chunk_size,
        .is_last_chunk = payload.is_last_chunk};

    // now take the chunk data
    std::string data = client.receiveSome(payload.chunk_size);

    chunk_handler.save_chunk(chunk_md, data);

    if (!chunk_md.is_last_chunk)
    {
        Message &&msg = messenger.receive_json_message();

        auto new_payload = std::get_if<ModifiedChunkPayload>(&(msg.payload));

        while (new_payload)
        {
            // now take the chunk data
            std::string data = client.receiveSome(new_payload->chunk_size);

            chunk_handler.save_chunk(chunk_md, data);

            msg = messenger.receive_json_message();
            new_payload = std::get_if<ModifiedChunkPayload>(&(msg.payload));

            if (new_payload->is_last_chunk)
                break;
        }
    }

    chunk_handler.finalize_file(working_dir + "/" + payload.filename);
}

void ServerMessageHandler::process_add_remove_chunk(const AddRemoveChunkPayload &payload, const bool is_removed)
{
    ChunkHandler chunk_handler(payload.filename);

    ChunkMetadata chunk_md{
        .op_type = is_removed ? OP_TYPE::REMOVE : OP_TYPE::ADD,
        .offset = payload.offset,
        .chunk_size = payload.chunk_size,
        .old_chunk_size = 0,
        .is_last_chunk = payload.is_last_chunk};

    // now take the chunk data
    std::string data = client.receiveSome(payload.chunk_size);

    chunk_handler.save_chunk(chunk_md, data);

    if (!chunk_md.is_last_chunk)
    {
        Message &&msg = messenger.receive_json_message();

        auto new_payload = std::get_if<AddRemoveChunkPayload>(&(msg.payload));

        while (new_payload)
        {
            // now take the chunk data
            std::string data = client.receiveSome(new_payload->chunk_size);

            chunk_handler.save_chunk(chunk_md, data);

            msg = messenger.receive_json_message();
            new_payload = std::get_if<AddRemoveChunkPayload>(&(msg.payload));

            if (new_payload->is_last_chunk)
                break;
        }
    }

    chunk_handler.finalize_file(working_dir + "/" + payload.filename);
}

void ServerMessageHandler::process_file_chunk(const SendChunkPayload &payload)
{
}

void ServerMessageHandler::process_fetch_files(const std::vector<std::string> &files)
{
}

// void ServerMessageHandler::process_fetch_modified_chunks() {}