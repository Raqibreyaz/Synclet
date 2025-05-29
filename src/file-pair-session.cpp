#include "../include/file-pair-session.hpp"

FilePairSession::FilePairSession(const std::string &filepath)
    : original_filepath(filepath),
      temp_filepath(filepath + ".incoming") {}

// point original_file and temp_file ptr to current given file
void FilePairSession::ensure_files_open()
{
    if (!original_file)
    {
        original_file = std::make_unique<FileIO>(original_filepath, std::ios::in);
    }

    if (!temp_file)
    {
        temp_file = std::make_unique<FileIO>(temp_filepath, std::ios::out | std::ios::app);
    }
}

// point original_file and temp_file ptr to new file, if given
void FilePairSession::reset_if_filepath_changes(const std::string &new_filepath)
{

    if (original_filepath != new_filepath)
    {
        close_session();
        original_filepath = new_filepath;
        temp_filepath = original_filepath + ".incoming";
        cursor = 0;
    }
    ensure_files_open();
}

// TODO: optimise in case of filling large gap
// Fills gap in temp_file if present from the offset
void FilePairSession::fill_gap_till_offset(const size_t offset)
{
    // skip if there is no gap to be filled
    if (offset <= cursor)
        return;

    // data copied to be filled
    std::string to_copy = original_file->read_file_from_offset(cursor, offset);

    // as we have moved BYTES forward after copying BYTES data
    cursor = offset;

    // now fill the data to temp_file
    temp_file->append_chunk(to_copy);
}

// chunk_size can be != chunk.size as in case of modify we will be replacing data of differet size
void FilePairSession::append_data(const std::string &chunk, const size_t chunk_size)
{
    temp_file->append_chunk(chunk);
    cursor += chunk_size;
}

// skip the given chunk from writing to temp_file
void FilePairSession::skip_removed_chunk(const size_t offset, const size_t chunk_size)
{
    cursor = offset + chunk_size;
}

// TODO: optimise in case of filling large remaining data
// fill rest of the data from original file to temp_file + rename temp_file to original
void FilePairSession::finalize_and_replace()
{
    fill_gap_till_offset(original_file->get_file_size() - 1);

    fs::remove(original_filepath);
    fs::rename(temp_filepath, original_filepath);

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