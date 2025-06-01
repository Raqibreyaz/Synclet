#include "../include/chunk-handler.hpp"

ChunkHandler::ChunkHandler(const std::string &filename) : dir_name(filename + "_dir"), filename(filename)
{
    // if directory not exists then create it
    if (!std::filesystem::exists(dir_name))
        std::filesystem::create_directory(dir_name);
}

void ChunkHandler::save_chunk(ChunkMetadata &chunk_md, const std::string &chunk_data)
{
    std::string filepath = std::format("{}/chunk-{}.bin",
                                       dir_name,
                                       chunk_md.offset);

    // open the file
    std::ofstream chunk_file(filepath);

    // write the metadata
    chunk_file.write(reinterpret_cast<char *>(&chunk_md), sizeof(chunk_md));

    // write the data
    chunk_file.write(chunk_data.c_str(), chunk_data.size());

    // now close the chunk_file
    chunk_file.close();
}

std::pair<ChunkMetadata, std::string> ChunkHandler::read_chunk_file(uint64_t offset)
{

    std::string filepath = std::format("{}/chunk-{}.bin",
                                       dir_name,
                                       offset);

    // open the file
    std::ifstream chunk_file(filepath);

    ChunkMetadata chunk_md;
    std::string chunk_data;

    // read the metadata
    chunk_file.read(reinterpret_cast<char *>(&chunk_md), sizeof(chunk_md));

    // read the data
    chunk_file.read(chunk_data.data(), chunk_data.size());

    // now close the chunk_file
    chunk_file.close();

    return std::make_pair(chunk_md, chunk_data);
}

// we have to take all the chunks sorted by their offset
void ChunkHandler::finalize_file(const std::string &original_filepath)
{
    FilePairSession file_session(original_filepath);

    // set to get offsets in sorted order
    std::set<uint64_t> sorted_offset;

    // insert all the offset in set
    for (auto &entry : fs::recursive_directory_iterator(dir_name))
    {
        auto filename = entry.path().string();
        auto dash = filename.find('-');
        auto dot = filename.find('.');
        uint64_t offset;

        std::from_chars(filename.data() + dash + 1, filename.data() + dot, offset);

        sorted_offset.insert(offset);
    }

    for (auto offset : sorted_offset)
    {
        auto &&[chunk_md, chunk_data] = read_chunk_file(offset);

        file_session.fill_gap_till_offset(offset);

        if (chunk_md.op_type == OP_TYPE::ADD)
            file_session.add_chunk(chunk_data, chunk_md.chunk_size);
        else if (chunk_md.op_type == OP_TYPE::REMOVE)
            file_session.skip_removed_chunk(chunk_md.offset, chunk_md.chunk_size);
        else
            file_session.add_chunk(chunk_data, chunk_md.old_chunk_size);
    }
    file_session.finalize_and_replace();
}