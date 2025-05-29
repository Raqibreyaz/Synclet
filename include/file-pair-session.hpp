#include <memory>
#include "file-io.hpp"

class FilePairSession
{
    std::unique_ptr<FileIO> original_file;
    std::unique_ptr<FileIO> temp_file;
    std::string original_filepath;
    std::string temp_filepath;

    // tells till where we reached reading the original file
    size_t cursor = 0;

public:
    FilePairSession(const std::string &filepath);
    void ensure_files_open();
    void reset_if_filepath_changes(const std::string &new_filepath);
    void fill_gap_till_offset(const size_t offset);
    void append_data(const std::string &chunk, const size_t chunk_size);
    void skip_removed_chunk(const size_t offset, const size_t chunk_size);
    void finalize_and_replace();
    void close_session();
    std::string get_filepath();
};