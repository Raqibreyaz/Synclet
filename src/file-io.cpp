#include "../include/file-io.hpp"

FileIO::FileIO() : fstream(nullptr) {};

FileIO::FileIO(const std::string &filepath)
{
    open_file(filepath);
}

// handle if file not exists then create condition
bool FileIO::open_file(const std::string &filepath)
{
    fstream.open(filepath, std::ios::in);

    if (!fstream)
    {
        std::cerr << "failed to open file " << filepath << std::endl;
        return false;
    }
    return true;
}

// create handler of removing bytes after index j

// create handler of replacing from index i to j with given data in a file

std::string FileIO::read_file_from_offset(const size_t offset, const size_t chunk_size)
{
    std::string chunk_string(chunk_size, '\0');

    // point to the required offset
    fstream.seekg(static_cast<std::streamoff>(offset));

    // read from that offset specific chunk size
    fstream.read(chunk_string.data(), chunk_size);

    return chunk_string;
}

std::string FileIO::read_file(const size_t n)
{
    return read_file_from_offset(0, n);
}

void FileIO::close_file()
{
    if (fstream.is_open())
        fstream.close();
}

FileIO::~FileIO()
{
    close_file();
}