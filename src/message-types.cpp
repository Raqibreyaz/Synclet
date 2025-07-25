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

FileSnapshot::FileSnapshot(const std::string &filename,
                           const uint64_t &file_size,
                           const std::time_t &mtime,
                           const std::unordered_map<std::string, ChunkInfo> &chunks)
    : filename(filename),
      file_size(file_size),
      mtime(mtime),
      chunks(chunks) {}

ModifiedChunkPayload::ModifiedChunkPayload() {}

ModifiedChunkPayload::ModifiedChunkPayload(const ChunkType chunk_type,
                                           const std::string &filename,
                                           const size_t offset,
                                           const size_t chunk_size,
                                           const size_t old_chunk_size,
                                           const bool is_last_chunk)
    : chunk_type(chunk_type),
      filename(filename),
      offset(offset),
      chunk_size(chunk_size),
      old_chunk_size(old_chunk_size),
      is_last_chunk(is_last_chunk) {}