#pragma once
#include <fstream>
#include <iostream>

class FileIO
{
    std::fstream fstream;

public:
    FileIO();
    FileIO(const std::string &filepath);
    bool open_file(const std::string &filepath);
    void close_file();
    std::string read_file_from_offset(const size_t offset, const size_t chunk_size);
    std::string read_file(const size_t n);
    ~FileIO();
};