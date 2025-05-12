#include "../include/synclet.hpp"

ChunkInfo::ChunkInfo() = default;

ChunkInfo::ChunkInfo(size_t offset, size_t size, const std::string &hash) : offset(offset), size(size), hash(hash) {}

FileSnapshot::FileSnapshot() = default;

FileSnapshot::FileSnapshot(const std::string &path, const uint64_t &size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks) : path(path), size(size), mtime(mtime), chunks(chunks) {}

std::time_t to_unix_timestamp(const fs::file_time_type &mtime)
{
    using namespace std::chrono;
    return system_clock::to_time_t(time_point_cast<system_clock::duration>(mtime - fs::file_time_type::clock::now() + system_clock::now()));
}

std::string create_hash(const std::vector<char> &data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

// Scan a directory and build a snapshot of all files and their chunks
std::vector<FileSnapshot> State::scan_directory(const std::string &dir, size_t chunk_size)
{
    // to store all the snapshots of files
    std::vector<FileSnapshot> snapshots;
    for (const auto &entry : fs::directory_iterator(dir))
    {

        //
        if (!entry.is_regular_file())
            continue;

        // open the file
        std::ifstream file(entry.path(), std::ios::binary);

        if (!file)
            throw std::runtime_error(std::format("{} {}", "failed to open file", entry.path().string()));

        // create a snapshot of the file
        FileSnapshot snapshot({entry.path(),
                               entry.file_size(),
                               to_unix_timestamp(fs::last_write_time(entry.path())),
                               {}});

        //  count total chunks for the file
        size_t noOfChunks = (entry.file_size() + chunk_size - 1) / chunk_size;

        // now collect ever chunk
        for (int i = 0; i < noOfChunks; i++)
        {
            // find the no of bytes to read
            int bytes_to_read = std::min(chunk_size, entry.file_size() - i * chunk_size);
            std::vector<char> buffer(bytes_to_read);

            // read that specified chunk
            file.read(buffer.data(), bytes_to_read);

            // store the chunk by hashing
            snapshot.chunks.emplace_back(i * chunk_size, bytes_to_read, create_hash(buffer));
        }

        // store the snapshot
        snapshots.push_back(std::move(snapshot));
        file.close();
    }

    return snapshots;
}

// Compare two snapshots and return changed/added/deleted chunks
void State::compare_snapshots(
    const std::vector<FileSnapshot> &old_snap,
    const std::vector<FileSnapshot> &new_snap,
    std::vector<FileSnapshot> &added,
    std::vector<FileSnapshot> &modified,
    std::vector<FileSnapshot> &deleted)
{
    // TODO: Implement snapshot comparison
}

// will save the snapshots as json in the file
bool State::save_snapshot(const std::string &filename, const std::vector<FileSnapshot> &snaps)
{
    json j = json::array();

    for (const auto &snap : snaps)
    {
        json file_json;
        file_json["path"] = snap.path;
        file_json["size"] = snap.size;
        file_json["mtime"] = snap.mtime;
        file_json["chunks"] = json::array();

        for (const auto &chunk : snap.chunks)
        {
            file_json["chunks"].push_back({{"offset", chunk.offset},
                                           {"size", chunk.size},
                                           {"hash", chunk.hash}});
        }
        j.push_back(file_json);
    }

    std::ofstream file(filename, std::ios::out);

    if (!file)
        throw std::runtime_error(std::format("failed to save snapshot to {}", filename));

    file << j.dump(4);
    file.close();

    return false;
}

// will load the saved file snaps from json
bool State::load_snapshot(const std::string &filename, std::vector<FileSnapshot> &snapshots)
{
    std::ifstream file(filename);

    json j;

    file >> j;

    for (const auto &file_json : j)
    {
        const std::string path = file_json["path"].get<std::string>();
        const uint64_t file_size = file_json["size"].get<uint64_t>();
        const std::time_t mtime = file_json["mtime"].get<std::time_t>();

        std::vector<ChunkInfo> chunks;

        for (const auto &chunk_json : file_json["chunks"])
        {
            ChunkInfo chunk;
            chunk.offset = chunk_json["offset"].get<size_t>();
            chunk.size = chunk_json["size"].get<size_t>();
            chunk.hash = chunk_json["hash"].get<std::string>();

            chunks.push_back(std::move(chunk));
        }

        snapshots.emplace_back(path, file_size, mtime, chunks);
    }

    return false;
}
