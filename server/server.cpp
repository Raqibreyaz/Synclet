// server.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../include/tcp-socket.hpp"
#include "../include/snapshot-manager.hpp"
#include "../include/messenger.hpp"
#include "../include/message-types.hpp"
#include "../include/message.hpp"

#define PORT 9000
#define DATA_DIR "./data"
#define SNAP_FILE "./snap-file.json"

std::function<void()> signal_handler = nullptr;
void signal_handler_wrap(int sig)
{
    if (signal_handler)
        signal_handler();
}

int main()
{
    SnapshotManager snap_manager(DATA_DIR, SNAP_FILE);

    // fetch the snaps from SNAP_FILE
    auto snaps = snap_manager.load_snapshot();

    if (snaps.empty())
        snaps = snap_manager.scan_directory();

    // create a server on localhost
    TcpServer server("127.0.0.1", std::to_string(PORT));
    TcpConnection client = server.acceptClient();
    Messenger messenger(client);

    signal_handler = [&client]()
    {
        client.closeConnection();
        exit(EXIT_SUCCESS);
    };

    signal(SIGINT, signal_handler_wrap);

    std::unique_ptr<FileIO> file_ptr = nullptr;
    std::unique_ptr<FileIO> temp_file_ptr = nullptr;

    // required for tracking , till which chunk we written
    size_t prev_old_chunk_end = 0;

    const auto file_ptr_initializer = [&](const std::string &filepath, const std::string &temp_filepath)
    {
        // point both file_ptr and temp_file_ptr to their correspoding files if not did
        if (!file_ptr)
        {
            file_ptr = std::make_unique<FileIO>(filepath, std::ios::in);
            prev_old_chunk_end = 0;
        }
        else if (file_ptr && file_ptr->get_filepath() != filepath)
        {
            file_ptr->close_file();
            file_ptr->open_file(filepath, std::ios::in);
            prev_old_chunk_end = 0;
        }
        if (!temp_file_ptr)
        {
            temp_file_ptr = std::make_unique<FileIO>(temp_filepath, std::ios::out | std::ios::app);
            prev_old_chunk_end = 0;
        }
        else if (temp_file_ptr && temp_file_ptr->get_filepath() != temp_filepath)
        {
            // before closing the prev file copy all the rest data to it
            temp_file_ptr->close_file();

            temp_file_ptr->open_file(temp_filepath, std::ios::out | std::ios::app);
            prev_old_chunk_end = 0;
        }
    };

    const auto copy_nbytes_between_files = [&](size_t n)
    {
        if (n > 0)
        {
            std::string to_copy = file_ptr->read_file_from_offset(prev_old_chunk_end, n);

            // as we have moved BYTES forward after copying BYTES data
            prev_old_chunk_end += n;

            temp_file_ptr->append_chunk(to_copy);
        }
    };

    const auto handle_last_chunk = [&](const std::string &filepath, const std::string &temp_filepath)
    {
        size_t n = file_ptr->get_file_size() > prev_old_chunk_end ? file_ptr->get_file_size() - prev_old_chunk_end : 0;
        copy_nbytes_between_files(n);

        fs::remove(filepath);
        fs::rename(temp_filepath, filepath);

        file_ptr = nullptr;
        temp_file_ptr = nullptr;
    };

    while (true)
    {
        Message &&msg = messenger.receive_json_message();

        std::clog << "message type is: " << message_type_to_string(msg.type);

        if (msg.type == MessageType::FILE_CREATE)
        {
            auto payload =
                std::get_if<FileCreateRemovePayload>(&(msg.payload));
            auto filepath = std::string(DATA_DIR) + "/" + payload->filename;
            if (!file_ptr)
                file_ptr = std::make_unique<FileIO>(filepath, std::ios::out);
            else
            {
                file_ptr->close_file();
                file_ptr->open_file(filepath, std::ios::out);
            }
        }
        else if (msg.type == MessageType::FILE_REMOVE)
        {
            auto payload =
                std::get_if<FileCreateRemovePayload>(&(msg.payload));
            auto filepath = std::string(DATA_DIR) + "/" + payload->filename;
            if (file_ptr && file_ptr->get_filepath() == filepath)
            {
                file_ptr->close_file();
                file_ptr = nullptr;
            }

            // before removing check if file exists
            if (fs::exists(filepath))
                fs::remove(filepath);
        }
        else if (msg.type == MessageType::FILE_RENAME)
        {
            auto payload =
                std::get_if<FileRenamePayload>(&(msg.payload));
            auto old_filepath = std::string(DATA_DIR) + "/" + payload->old_filename;
            auto new_filepath = std::string(DATA_DIR) + "/" + payload->new_filename;

            fs::rename(old_filepath, new_filepath);

            if (!file_ptr)
                file_ptr = std::make_unique<FileIO>(new_filepath, std::ios::out);

            else
            {
                file_ptr->close_file();
                file_ptr->open_file(new_filepath, std::ios::out);
            }
        }
        else if (msg.type == MessageType::MODIFIED_CHUNK)
        {
            auto payload =
                std::get_if<ModifiedChunkPayload>(&(msg.payload));

            auto filepath = std::string(DATA_DIR) + "/" + payload->filename;

            auto temp_filepath = filepath + ".incoming";

            file_ptr_initializer(filepath, temp_filepath);

            if (payload->offset < prev_old_chunk_end)
            {
                std::cerr << "problem found offset is smaller: " << payload->offset << " < " << prev_old_chunk_end << std::endl;
                continue;
            }

            // if there is a gap between current prev_old_chunk_end then
            // we have to copy bytes from original
            size_t bytes_to_copy = payload->offset > prev_old_chunk_end ? payload->offset - prev_old_chunk_end : 0;

            copy_nbytes_between_files(bytes_to_copy);

            // now take the chunk data
            std::string data = client.receiveSome(payload->chunk_size);

            // now write the chunk data
            temp_file_ptr->append_chunk(data);

            // now go to next part
            prev_old_chunk_end = payload->offset + payload->old_chunk_size;

            // if this is the last chunk then append all rest of the data now
            if (payload->is_last_chunk)
                handle_last_chunk(filepath, temp_filepath);
        }
        else
        {
            auto payload =
                std::get_if<AddRemoveChunkPayload>(&(msg.payload));

            auto filepath = std::string(DATA_DIR) + "/" + payload->filename;
            auto temp_filepath = filepath + ".incoming";

            file_ptr_initializer(filepath, temp_filepath);

            // if there is a gap between current prev_old_chunk_end then
            // we have to copy bytes from original
            size_t bytes_to_copy = payload->offset > prev_old_chunk_end ? payload->offset - prev_old_chunk_end : 0;

            if (payload->offset < prev_old_chunk_end)
            {
                std::cerr << "problem found in adding chunk: offset " << payload->offset << " < " << "file ptr" << std::endl;
                continue;
            }

            // copy required bytes of data to temp file
            copy_nbytes_between_files(bytes_to_copy);

            // fetch data when chunk is added
            bool to_skip = msg.type == MessageType::REMOVED_CHUNK ? true : false;
            if (!to_skip)
            {
                std::string data = client.receiveSome(payload->chunk_size);
                temp_file_ptr->append_chunk(data);
            }
            // chunk was not present previosuly so we cant move the pointer
            else
                prev_old_chunk_end = payload->offset + payload->chunk_size;

            // if this is the last chunk then append all rest of the data now
            if (payload->is_last_chunk)
                handle_last_chunk(filepath, temp_filepath);
        }
    }
    return 0;
}
