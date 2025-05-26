#include "../include/file-io.hpp"

FileIO::FileIO(const std::string &filepath, std::ios::openmode mode)
{
    open_file(filepath, mode);
}

bool FileIO::open_file(const std::string &filepath, std::ios::openmode mode)
{
    this->filepath = filepath;

    fstream.open(filepath, mode | std::ios::binary);

    if (!fstream)
    {
        std::cerr << "failed to open file " << filepath << std::endl;
        return false;
    }
    return true;
}

void FileIO::close_file()
{
    if (fstream.is_open())
        fstream.close();
}

std::string FileIO::read_file_from_offset(const size_t offset, const size_t chunk_size)
{
    if (!fstream || !(fstream.is_open()))
        throw std::runtime_error("file need to be opened for reading!");

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

// write at a particular offset in a file
void FileIO::write_file_at_offset(const size_t offset, const std::string &data)
{
    if (!fstream || !(fstream.is_open()))
        throw std::runtime_error("file need to be opened for writing!");

    fstream.seekp(offset);
    fstream.write(data.c_str(), data.size());
}

// write the chunk at last of file
void FileIO::append_chunk(const std::string &data)
{
    if (!fstream || !(fstream.is_open()))
        throw std::runtime_error("file need to be opened for appending!");

    fstream.write(data.c_str(), data.size());
}

std::string FileIO::get_filepath()
{
    return this->filepath;
}

uintmax_t FileIO::get_file_size()
{
    return fs::file_size(filepath);
}

FileIO::~FileIO()
{
    close_file();
}