#include "../include/synclet.hpp"
#include <deque>

ChunkInfo::ChunkInfo() = default;

ChunkInfo::ChunkInfo(size_t offset, size_t size, const std::string &hash) : offset(offset), size(size), hash(hash) {}

FileSnapshot::FileSnapshot() = default;

FileSnapshot::FileSnapshot(const uint64_t &size, const std::time_t &mtime, const std::vector<ChunkInfo> &chunks) : size(size), mtime(mtime), chunks(chunks) {}

std::time_t to_unix_timestamp(const fs::file_time_type &mtime)
{
    using namespace std::chrono;
    return system_clock::to_time_t(time_point_cast<system_clock::duration>(mtime - fs::file_time_type::clock::now() + system_clock::now()));
}

std::string create_hash(const std::vector<char> &data)
{
    std::deque<char> dq;

    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

// creates snapshot of the file using content dependent chunking
FileSnapshot createSnapshot(const std::string &file_path, const uint64_t file_size, const time_t last_write_time)
{
    // open the file
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
        throw std::runtime_error(std::format("{} {}", "failed to open file", file_path));

    // create a snapshot of the file
    FileSnapshot snapshot(file_size, last_write_time, {});

    // min window size: 32
    // max window size: 128
    const int w_size = std::clamp<int>(file_size / 1000000, 32, 128);

    // min n: 2048
    const int n = std::max(2048, static_cast<int>(file_size / (512 * 1024)));

    const int modulo = 1e9 + 7;
    const int base = 256;

    // for computing hash
    long long hash = 0;
    size_t chunk_start = 0;

    // calculating highest power to avoid recomputation
    long long highest_power = 1;
    for (int i = 0; i < w_size - 1; i++)
        highest_power = (highest_power * base) % modulo;

    std::deque<char> window;
    std::vector<char> chunk;

    const auto roll_hash = [&](char c)
    {
        hash = (1LL * hash * base + c) % modulo;
        window.push_back(c);
    };

    // process a character each time
    char c = '\0';
    while (file.get(c))
    {
        chunk.push_back(c);

        // when the window is small then complete it
        if (window.size() >= w_size)
        {
            // take out the first char from window and hash
            char out_char = window.front();
            window.pop_front();
            long long removed = 1LL * out_char * highest_power % modulo;
            hash = (1LL * hash - removed + modulo) % modulo;
        }

        // add the char to hash and window
        roll_hash(c);

        // when a chunk boundary found then process it
        if (window.size() >= w_size && hash % n == 0)
        {
            snapshot.chunks.emplace_back(chunk_start, chunk.size(), create_hash(chunk));

            // move to next chunk
            chunk_start += chunk.size();
            chunk.clear();
            window.clear();
            hash = 0;
        }
    };

    if (!chunk.empty())
        snapshot.chunks.emplace_back(chunk_start, chunk.size(), create_hash(chunk));

    return snapshot;
}

// Scan a directory and build a snapshot of all files and their chunks
std::unordered_map<std::string, FileSnapshot> State::scan_directory(const std::string &dir)
{
    // to store all the snapshots of files
    std::unordered_map<std::string, FileSnapshot> snapshots;
    for (const auto &entry : fs::directory_iterator(dir))
    {

        // file should be normal file , !socket, !directory
        if (!entry.is_regular_file())
            continue;

        FileSnapshot snapshot = createSnapshot(entry.path(), entry.file_size(), to_unix_timestamp(fs::last_write_time(entry.path())));

        // store the snapshot
        snapshots[entry.path().string()] = std::move(snapshot);
    }

    return snapshots;
}

// Compare two snapshots and return changed/added/deleted chunks
void State::compare_snapshots(
    const std::unordered_map<std::string, FileSnapshot> &currSnapshot, const std::unordered_map<std::string, FileSnapshot> &prevSnapshot)
{
    // to track if anything is changed
    bool isChanged = false;

    // check for file addition & modification
    for (const auto &[path, snap] : currSnapshot)
    {
        const auto prev_snap = prevSnapshot.find(path);

        // check for file addition
        if (prev_snap == prevSnapshot.end())
        {
            std::clog << std::format("{} file is added", path) << std::endl;
            isChanged = true;
        }

        // check for file modification
        else if ((snap.size != prev_snap->second.size || snap.mtime != prev_snap->second.mtime) && State::check_file_modification(snap, prev_snap->second))
        {
            std::clog << std::format("in {}", path) << std::endl;
            isChanged = true;
        }
    }

    // check for file deletion
    for (const auto &[path, snap] : prevSnapshot)
        if (currSnapshot.find(path) == currSnapshot.end())
        {
            std::clog << std::format("{} file is deleted!", path) << std::endl;
            isChanged = true;
        }

    if (!isChanged)
        std::clog << "no changes found" << std::endl;
}

bool State::check_file_modification(const FileSnapshot &file_curr_snap, const FileSnapshot &file_prev_snap)
{
    bool isChanged = false;
    for (size_t i = 0; i < std::min(file_curr_snap.chunks.size(), file_prev_snap.chunks.size()); i++)
    {
        ChunkInfo chunk_a = file_curr_snap.chunks[i];
        ChunkInfo chunk_b = file_prev_snap.chunks[i];

        if (chunk_a.hash != chunk_b.hash)
        {
            std::clog << "offset_1: " << chunk_a.offset << "\toffset_2: " << chunk_b.offset << std::endl;
            std::clog << "size_1: " << chunk_a.size << "\tsize_2: " << chunk_b.size << std::endl;

            std::clog << std::format("[{},{}] changed", chunk_a.offset, chunk_a.size) << std::endl;
            std::clog << "chunks not matched : " << std::endl
                      << "start_1: " << chunk_a.offset << "\tstart_2: " << chunk_b.offset << std::endl
                      << "end_1: " << chunk_a.offset + chunk_a.size << "\tend_2: " << chunk_b.offset + chunk_b.size << std::endl;
            isChanged = true;
        }
    }
    return isChanged;
}

// will save the snapshots as json in the file
void State::save_snapshot(const std::string &filename, const std::unordered_map<std::string, FileSnapshot> &snaps)
{
    json j = json::array();

    for (const auto &[path, snap] : snaps)
    {
        json file_json;
        file_json["path"] = path;
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
}

// will load the saved file snaps from json
std::unordered_map<std::string, FileSnapshot> State::load_snapshot(const std::string &filename)
{
    std::ifstream file(filename);

    std::unordered_map<std::string, FileSnapshot> snapshots;

    if (!file.is_open())
    {
        std::cerr << "snap file file does not exist, returning default values" << std::endl;
        return snapshots;
    }
    if (fs::file_size(filename) == 0)
    {
        std::cerr << "snap file is empty, returning default values" << std::endl;
        return snapshots;
    }

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

        snapshots[path] = FileSnapshot(file_size, mtime, chunks);
    }

    return snapshots;
}
