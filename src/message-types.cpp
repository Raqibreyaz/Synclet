#include "../include/message-types.hpp"

ChunkInfo::ChunkInfo() = default;

ChunkInfo::ChunkInfo(size_t offset, size_t chunk_size, const std::string &hash, int chunk_no)
    : offset(offset),
      chunk_size(chunk_size),
      hash(hash),
      chunk_no(chunk_no)
{
}

FileSnapshot::FileSnapshot() = default;

FileSnapshot::FileSnapshot(const std::string &filename, const uint64_t &file_size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks)
    : filename(filename),
      file_size(file_size),
      mtime(mtime),
      chunks(chunks) {}
