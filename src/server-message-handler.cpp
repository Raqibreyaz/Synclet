#include "../include/server-message-handler.hpp"

ServerMessageHandler::ServerMessageHandler(const std::string &working_dir, TcpConnection &client)
    : working_dir(working_dir),
      client(client) {}

// returns the session of that particular file, creates one if not present
void ServerMessageHandler::get_or_create_session(const std::string &filename)
{
    const std::string filepath = working_dir + "/" + filename;

    if (!active_session)
        active_session = std::make_unique<FilePairSession>(filepath);

    active_session->reset_if_filepath_changes(filepath);
    active_session->ensure_files_open();
}

void ServerMessageHandler::process_create_file(const FileCreateRemovePayload &payload)
{
    // will create a file of this name
    get_or_create_session(payload.filename);
}

void ServerMessageHandler::process_delete_file(const FileCreateRemovePayload &payload)
{
    auto filepath = working_dir + "/" + payload.filename;

    // close the file if it is opened
    if (active_session && active_session->get_filepath() == filepath)
        active_session->close_session();

    // before removing check if file exists
    if (fs::exists(filepath))
        fs::remove(filepath);
}

void ServerMessageHandler::process_file_rename(const FileRenamePayload &payload)
{
    auto old_filepath = working_dir + "/" + payload.old_filename;
    auto new_filepath = working_dir + "/" + payload.new_filename;

    // point to the new filepath now
    if (active_session && active_session->get_filepath() == old_filepath)
        active_session->reset_if_filepath_changes(new_filepath);

    fs::rename(old_filepath, new_filepath);
}

void ServerMessageHandler::process_modified_chunk(const ModifiedChunkPayload &payload)
{
    // initialize the active_sesion var with required params
    get_or_create_session(payload.filename);

    // ensure that original file ptr and temp file ptr are pointing to required file
    active_session->ensure_files_open();

    // fill gap if present
    active_session->fill_gap_till_offset(payload.offset);

    // now take the chunk data
    std::string data = client.receiveSome(payload.chunk_size);

    // since cursor is pointing to old_file so we will move according to oldChunkSize
    active_session->append_data(data, payload.old_chunk_size);

    // handle the last chunk by integrating the rest of the data
    if (payload.is_last_chunk)
        active_session->finalize_and_replace();
}

void ServerMessageHandler::process_add_remove_chunk(const AddRemoveChunkPayload &payload, const bool is_removed)
{
    get_or_create_session(payload.filename);

    active_session->ensure_files_open();

    active_session->fill_gap_till_offset(payload.offset);

    // skip the chunk on writing the file if removed
    if (is_removed)
        active_session->skip_removed_chunk(payload.offset, payload.chunk_size);

    // in case of add then fetch data and add chunk
    if (!is_removed)
    {
        std::string data = client.receiveSome(payload.chunk_size);
        active_session->append_data(data, payload.chunk_size);
    }

    // if this is the last chunk then append all rest of the data now
    if (payload.is_last_chunk)
        active_session->finalize_and_replace();
}