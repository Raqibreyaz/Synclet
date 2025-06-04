#include "../include/file-pair-session.hpp"

FilePairSession::FilePairSession(const std::string &filepath, const bool append_to_original)
    : original_filepath(filepath),
      append_to_original(append_to_original)
{
    if (!append_to_original)
        temp_filepath = filepath + ".incoming";
}

// point original_file and temp_file ptr to current given file
void FilePairSession::ensure_files_open()
{
    if (!original_file)
    {
        std::ios::openmode mode = append_to_original ? std::ios::app : std::ios::in;
        original_file = std::make_unique<FileIO>(original_filepath, mode);
    }

    if (!temp_file && !append_to_original)
    {
        temp_file = std::make_unique<FileIO>(temp_filepath, std::ios::out | std::ios::app);
    }
}

// point original_file and temp_file ptr to new file, if given
void FilePairSession::reset_if_filepath_changes_append_required(const std::string &new_filepath, const bool append_to_original)
{

    if (original_filepath != new_filepath ||
        this->append_to_original != append_to_original)
    {
        close_session();
        original_filepath = new_filepath;

        if (!append_to_original)
            temp_filepath = original_filepath + ".incoming";

        cursor = 0;
    }
    ensure_files_open();
}

// Fills gap in temp_file if present from the offset
void FilePairSession::fill_gap_till_offset(const size_t offset)
{
    // skip if there is no gap to be filled or temp_file not exists
    if (offset <= cursor || !temp_file)
        return;

    size_t chunk_size = offset - cursor;

    for (size_t read_so_far = 0; read_so_far < chunk_size;)
    {
        const std::string &to_copy = original_file->read_file_from_offset(
            cursor + read_so_far,
            std::min(MAX_READ_SIZE, chunk_size - read_so_far));

        if (to_copy.empty())
            break;

        temp_file->append_chunk(to_copy);

        read_so_far += to_copy.size();
    }

    // as we have moved BYTES forward after copying BYTES data
    cursor = offset;
}

// append chunk data to original file
void FilePairSession::append_data(const std::string &chunk)
{
    if (is_appending_to_original())
        original_file->append_chunk(chunk);
}

// chunk_size can be != chunk.size as in case of modify we will be replacing data of differet size
void FilePairSession::add_chunk(const std::string &chunk, const size_t chunk_size, const bool is_new_chunk)
{
    if (!temp_file)
    {
        std::cerr << "failed to add chunk! temp_file not exists" << std::endl;
        return;
    }

    temp_file->append_chunk(chunk);

    // since the chunk was not previously present so no need to move the original file cursor
    if (!is_new_chunk)
        cursor += chunk_size;
}

// skip the given chunk from writing to temp_file
void FilePairSession::skip_removed_chunk(const size_t offset, const size_t chunk_size)
{
    cursor = offset + chunk_size;
}

// fill rest of the data from original file to temp_file + rename temp_file to original
void FilePairSession::finalize_and_replace()
{
    if (temp_file)
    {
        fill_gap_till_offset(original_file->get_file_size());

        fs::remove(original_filepath);
        fs::rename(temp_filepath, original_filepath);
    }

    close_session();
}

// close the original_file and temp_file ptr
void FilePairSession::close_session()
{
    if (original_file)
    {
        original_file->close_file();
        original_file.reset();
    }
    if (temp_file)
    {
        temp_file->close_file();
        temp_file.reset();
    }
}

std::string FilePairSession::get_filepath()
{
    return original_filepath;
}

bool FilePairSession::is_appending_to_original()
{
    return append_to_original;
}

FilePairSession::~FilePairSession()
{
    close_session();
}