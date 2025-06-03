#include "../include/chunk-handler.hpp"

ChunkHandler::ChunkHandler(const std::string &filename) : dir_name(filename + "_dir"), filename(filename)
{
    // if directory not exists then create it
    if (!std::filesystem::exists(dir_name))
        std::filesystem::create_directory(dir_name);
}

// save the given chunk metadata + data in that corresponding file
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

// read a particular chunk's metadata + data using offset
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

    // now resize the chunk_data var for getting whole chunk data
    chunk_data.resize(chunk_md.chunk_type == ChunkType::MODIFY
                          ? chunk_md.old_chunk_size
                          : chunk_md.chunk_size);

    // read the data
    if (chunk_md.chunk_type != ChunkType::REMOVE)
        chunk_file.read(chunk_data.data(), chunk_data.size());

    // now close the chunk_file
    chunk_file.close();

    return std::make_pair(chunk_md, chunk_data);
}

// requires a original filepath to replace it with the new file
void ChunkHandler::finalize_file(const std::string &original_filepath)
{
    FilePairSession file_session(original_filepath);
    file_session.ensure_files_open();

    // set to get offsets in sorted order
    std::set<uint64_t> sorted_offset;

    // insert all the offset in set
    for (auto &entry : fs::recursive_directory_iterator(dir_name))
    {
        auto filename = extract_filename_from_path(entry.path().string());
        auto dash = filename.find('-');
        auto dot = filename.find('.');
        uint64_t offset;

        std::from_chars(filename.data() + dash + 1, filename.data() + dot, offset);

        sorted_offset.insert(offset);
    }

    // now take each file via sorted offset
    for (auto offset : sorted_offset)
    {
        // read the chunk file
        const auto &[chunk_md, chunk_data] = read_chunk_file(offset);

        // fill gap in temp_file using original_file data till offset reach
        file_session.fill_gap_till_offset(offset);

        // if chunk was added then add the chunk to temp file
        if (chunk_md.chunk_type == ChunkType::ADD)
            file_session.add_chunk(chunk_data, chunk_md.chunk_size, true);

        // if chunk was removed then skip it copying from original and just move pointer forward by it's size
        else if (chunk_md.chunk_type == ChunkType::REMOVE)
            file_session.skip_removed_chunk(chunk_md.offset, chunk_md.chunk_size);

        // if chunk was modified then add the chunk to temp_file and move pointer forward by size in the original
        else
            file_session.add_chunk(chunk_data, chunk_md.old_chunk_size, false);
    }

    // after placing all the chunks finalize the temp_file with the rest data from original_file
    file_session.finalize_and_replace();

    // now remove the whole temp_directory of the file
    std::cout << fs::remove_all(dir_name) << " files deleted from dir: " << dir_name << std::endl;

    file_session.close_session();
}