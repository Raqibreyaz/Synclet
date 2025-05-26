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

AddRemoveChunkPayload::AddRemoveChunkPayload()
    : filename(""),
      offset(SIZE_MAX),
      chunk_size(SIZE_MAX),
      is_last_chunk(false) {}

AddRemoveChunkPayload::AddRemoveChunkPayload(const std::string &filename,
                                             const size_t offset,
                                             const size_t chunk_size,
                                             const bool is_last_chunk)
    : filename(filename),
      offset(offset),
      chunk_size(chunk_size),
      is_last_chunk(is_last_chunk) {}

ModifiedChunkPayload::ModifiedChunkPayload()
    : old_chunk_size(SIZE_MAX) {}

ModifiedChunkPayload::ModifiedChunkPayload(const std::string &filename,
                                           const size_t offset,
                                           const size_t chunk_size,
                                           const size_t old_chunk_size,
                                           const bool is_last_chunk)
    : AddRemoveChunkPayload(filename,
                            offset,
                            chunk_size,
                            is_last_chunk),
      old_chunk_size(old_chunk_size) {}