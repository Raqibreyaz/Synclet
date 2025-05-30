#pragma once
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

class FileIO
{
    std::fstream fstream;
    std::string filepath;

public:
    FileIO(const std::string &filepath,const std::ios::openmode mode = std::ios::in);
    bool open_file(const std::string &filepath,const std::ios::openmode mode);
    void close_file();
    std::string read_file_from_offset(const size_t offset, const size_t chunk_size);
    std::string read_file(const size_t n);
    void write_file_at_offset(const size_t offset, const std::string &data);
    void append_chunk(const std::string &data);
    std::string get_filepath();
    uintmax_t get_file_size();
    ~FileIO();
};