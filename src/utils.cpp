#include "../include/utils.hpp"

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

// TODO: optimise instead of getting single char from file get more
// creates snapshot of the file using content dependent chunking
FileSnapshot createSnapshot(const std::string &file_path, const uint64_t file_size, const time_t last_write_time)
{
    // open the file
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
        throw std::runtime_error(std::format("{} {}", "failed to open file", file_path));

    size_t last_slash = file_path.find_last_of('/');
    if (last_slash == std::string::npos)
        last_slash = 0;

    // create a snapshot of the file
    FileSnapshot snapshot(file_path.substr(last_slash + 1), file_size, last_write_time, {});

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
    int chunk_no = 0;
    while (file.get(c))
    {
        chunk.push_back(c);

        // when the window is small then complete it
        if (window.size() >= static_cast<size_t>(w_size))
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
        if (window.size() >= static_cast<size_t>(w_size) && hash % n == 0)
        {
            snapshot.chunks.emplace_back(chunk_start, chunk.size(), create_hash(chunk), chunk_no);

            // move to next chunk
            chunk_start += chunk.size();
            chunk.clear();
            window.clear();
            hash = 0;
            chunk_no++;
        }
    };

    if (!chunk.empty())
        snapshot.chunks.emplace_back(chunk_start, chunk.size(), create_hash(chunk), chunk_no);

    return snapshot;
}

// Scan a directory and build a snapshot of all files and their chunks
std::unordered_map<std::string, FileSnapshot> scan_directory(const std::string &dir)
{
    // to store all the snapshots of files
    std::unordered_map<std::string, FileSnapshot> snapshots;
    for (const auto &entry : fs::directory_iterator(dir))
    {

        // file should be normal file , !socket, !directory
        if (!entry.is_regular_file())
            continue;

        // full path requird for opening file
        FileSnapshot snapshot = createSnapshot(entry.path(), entry.file_size(), to_unix_timestamp(fs::last_write_time(entry.path())));

        // fidn the last slash to extract filename
        size_t lastSlash = entry.path().string().find_last_of('/');
        if (lastSlash == std::string::npos)
            lastSlash = -1;

        // store the snapshot
        snapshots[entry.path().string().substr(lastSlash + 1)] = std::move(snapshot);
    }

    return snapshots;
}

// Compare two snapshots and return changed/added/deleted chunks
void compare_snapshots(
    const std::unordered_map<std::string, FileSnapshot> &currSnapshot, const std::unordered_map<std::string, FileSnapshot> &prevSnapshot)
{
    // to track if anything is changed
    bool isChanged = false;

    // check for file addition & modification
    for (const auto &[filename, snap] : currSnapshot)
    {
        const auto prev_snap = prevSnapshot.find(filename);

        // check for file addition
        if (prev_snap == prevSnapshot.end())
        {
            std::clog << std::format("{} file is added", filename) << std::endl;
            isChanged = true;
        }

        // check for file modification
        else if ((snap.file_size != prev_snap->second.file_size || snap.mtime != prev_snap->second.mtime) && check_file_modification(snap, prev_snap->second))
        {
            std::clog << std::format("in {}", filename) << std::endl;
            isChanged = true;
        }
    }

    // check for file deletion
    for (const auto &[filename, snap] : prevSnapshot)
        if (currSnapshot.find(filename) == currSnapshot.end())
        {
            std::clog << std::format("{} file is deleted!", filename) << std::endl;
            isChanged = true;
        }

    if (!isChanged)
        std::clog << "no changes found" << std::endl;
}

// TODO: optimise for better change detection
bool check_file_modification(const FileSnapshot &file_curr_snap, const FileSnapshot &file_prev_snap)
{
    bool isChanged = false;
    for (size_t i = 0; i < std::min(file_curr_snap.chunks.size(), file_prev_snap.chunks.size()); i++)
    {
        ChunkInfo chunk_a = file_curr_snap.chunks[i];
        ChunkInfo chunk_b = file_prev_snap.chunks[i];

        if (chunk_a.hash != chunk_b.hash)
        {
            std::clog << "offset_1: " << chunk_a.offset << "\toffset_2: " << chunk_b.offset << std::endl;
            std::clog << "size_1: " << chunk_a.chunk_size << "\tsize_2: " << chunk_b.chunk_size << std::endl;

            std::clog << std::format("[{},{}] changed", chunk_a.offset, chunk_a.chunk_size) << std::endl;
            std::clog << "chunks not matched : " << std::endl
                      << "start_1: " << chunk_a.offset << "\tstart_2: " << chunk_b.offset << std::endl
                      << "end_1: " << chunk_a.offset + chunk_a.chunk_size << "\tend_2: " << chunk_b.offset + chunk_b.chunk_size << std::endl;
            isChanged = true;
        }
    }
    return isChanged;
}

// will save the snapshots as json in the file
void save_snapshot(const std::string &filename, const std::unordered_map<std::string, FileSnapshot> &snaps)
{
    json j = json::array();

    for (const auto &[filename, snap] : snaps)
    {
        json file_json;
        file_json["filename"] = snap.filename;
        file_json["size"] = snap.file_size;
        file_json["mtime"] = snap.mtime;
        file_json["chunks"] = json::array();

        for (const auto &chunk : snap.chunks)
        {
            file_json["chunks"]
                .push_back({{"offset", chunk.offset},
                            {"size", chunk.chunk_size},
                            {"hash", chunk.hash},
                            {"chunk_no", chunk.chunk_no}});
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
std::unordered_map<std::string, FileSnapshot> load_snapshot(const std::string &filename)
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
        const std::string filename = file_json["filename"].get<std::string>();
        const uint64_t file_size = file_json["size"].get<uint64_t>();
        const std::time_t mtime = file_json["mtime"].get<std::time_t>();

        std::vector<ChunkInfo> chunks;

        for (const auto &chunk_json : file_json["chunks"])
        {
            ChunkInfo chunk;
            chunk.offset = chunk_json["offset"].get<size_t>();
            chunk.chunk_size = chunk_json["size"].get<size_t>();
            chunk.hash = chunk_json["hash"].get<std::string>();
            chunk.chunk_no = chunk_json["chunk_no"].get<int>();

            chunks.push_back(std::move(chunk));
        }

        snapshots[filename] = FileSnapshot(filename, file_size, mtime, chunks);
    }

    return snapshots;
}
