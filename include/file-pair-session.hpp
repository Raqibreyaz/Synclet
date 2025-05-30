#include <memory>
#include "file-io.hpp"

class FilePairSession
{
    std::unique_ptr<FileIO> original_file;
    std::unique_ptr<FileIO> temp_file;
    std::string original_filepath;
    std::string temp_filepath;
    const bool append_to_original;

    // tells till where we reached reading the original file
    size_t cursor = 0;

public:
    FilePairSession(const std::string &filepath, const bool append_to_original = false);
    void ensure_files_open();
    void reset_if_filepath_changes_append_required(const std::string &new_filepath,const bool append_to_original);
    void fill_gap_till_offset(const size_t offset);
    void append_data(const std::string &chunk);
    void add_chunk(const std::string &chunk, const size_t chunk_size);
    void skip_removed_chunk(const size_t offset, const size_t chunk_size);
    void finalize_and_replace();
    void close_session();
    std::string get_filepath();
    bool is_appending_to_original();
};