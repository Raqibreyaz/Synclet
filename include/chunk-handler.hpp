#pragma once
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <set>
#include <charconv>
#include "file-pair-session.hpp"
#include "message-types.hpp"

// disabling padding to align each one by one
#pragma pack(push, 1)
struct ChunkMetadata
{
    ChunkType chunk_type;
    uint64_t offset;
    uint64_t chunk_size;
    uint64_t old_chunk_size;
    uint8_t is_last_chunk;
};
#pragma pack(pop)

class ChunkHandler{
    public:
    ChunkHandler(const std::string& filename);
    void save_chunk(ChunkMetadata&,const std::string& chunk_data);
    void finalize_file(const std::string& original_filepath);

    private:
    std::string dir_name;
    std::string filename;
    std::pair<ChunkMetadata, std::string> read_chunk_file(uint64_t offset);
};

// example : ChunkMetadata chunk_md;
// chunk_md.op_type = OP_TYPE::ADD
// chunk_md.offset = offset;
// chunk_md.chunk_size = chunk_size;
// chunk_md.old_chunk_size = old_chunk_size;
// chunk_md.is_last_chunk = false;