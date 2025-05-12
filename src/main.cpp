#include <iostream>
#include "../include/synclet.hpp"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <directory>\n";
        return 1;
    }
    std::string dir = argv[1];

    auto snapshots = State::scan_directory(dir);
    for (const auto &file : snapshots)
    {
        std::cout << "File: " << file.path << "\n";
        for (const auto &chunk : file.chunks)
        {
            std::cout << "  Chunk offset: " << chunk.offset
                      << ", size: " << chunk.size
                      << ", hash: " << chunk.hash << "\n";
        }
    }
    return 0;
}