#include "../include/snapshot-manager.hpp"

SnapshotManager::SnapshotManager(const std::string &data_dir_path, const std::string &snap_file_path) : data_dir_path(data_dir_path), snap_file_path(snap_file_path) {}

std::string SnapshotManager::create_hash(const std::vector<char> &data)
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
FileSnapshot SnapshotManager::createSnapshot(const std::string &file_path)
{
    const uint64_t file_size = fs::file_size(file_path);
    const time_t last_write_time = to_unix_timestamp(fs::last_write_time(file_path));

    // open the file
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
        throw std::runtime_error(std::format("{} {}", std::string("failed to open file"), file_path));

    size_t last_slash = file_path.find_last_of('/');
    if (last_slash == std::string::npos)
        last_slash = -1;

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
DirSnapshot SnapshotManager::scan_directory()
{
    // to store all the snapshots of files
    DirSnapshot snapshots;
    for (const auto &entry : fs::directory_iterator(data_dir_path))
    {
        // file should be normal file , !socket, !directory
        if (!entry.is_regular_file())
            continue;

        // full path required for opening file
        FileSnapshot snapshot = createSnapshot(entry.path().string());

        // store the snapshot
        snapshots[extract_filename_from_path(entry.path().string())] = std::move(snapshot);
    }

    return snapshots;
}

// Compare two snapshots and return changed/added/deleted chunks
DirChanges SnapshotManager::compare_snapshots(
    const DirSnapshot &currSnapshot, const DirSnapshot &prevSnapshot)
{
    // to track if anything is changed
    DirChanges changes;

    // check for file addition & modification
    for (const auto &[filename, snap] : currSnapshot)
    {
        const auto prev_snap = prevSnapshot.find(filename);

        // check for file addition
        if (prev_snap == prevSnapshot.end())
        {
            std::clog << std::format("{} file is added", filename) << std::endl;
            changes.created_files.push_back(filename);
        }

        // check for file modification
        else if (snap.file_size != prev_snap->second.file_size || snap.mtime != prev_snap->second.mtime)
        {
            std::clog << std::format("in {}", filename) << std::endl;
            changes.modified_files.push_back(get_file_modification(snap, prev_snap->second));
        }
    }

    // check for file deletion
    for (const auto &[filename, snap] : prevSnapshot)
        if (currSnapshot.find(filename) == currSnapshot.end())
        {
            std::clog << std::format("{} file is deleted!", filename) << std::endl;
            changes.removed_files.push_back(filename);
        }

    if (changes.created_files.empty() && changes.modified_files.empty() && changes.removed_files.empty())
        std::clog << "no changes found" << std::endl;

    return changes;
}

// compare curr and prev snap of the file(using only hash) and get changes as modified, removed chunks
FileModification SnapshotManager::get_file_modification(const FileSnapshot &file_curr_snap, const FileSnapshot &file_prev_snap)
{

    FileModification changes{.filename = file_curr_snap.filename,
                             .added = {},
                             .modified = {},
                             .removed = {}};

    // for modified chunks
    size_t i;
    for (i = 0; i < std::min(file_curr_snap.chunks.size(), file_prev_snap.chunks.size()); ++i)
    {
        const ChunkInfo &chunk_a = file_curr_snap.chunks[i];
        const ChunkInfo &chunk_b = file_prev_snap.chunks[i];

        // for modified chunks add the start and end index of prev snap
        if (chunk_a.hash != chunk_b.hash)
        {
            ModifiedChunkPayload modified_chunk;
            modified_chunk.filename = file_prev_snap.filename,
            modified_chunk.new_start_index = chunk_a.offset,
            modified_chunk.new_end_index = chunk_a.offset + chunk_a.chunk_size - 1,
            modified_chunk.old_start_index = chunk_b.offset,
            modified_chunk.old_end_index = chunk_b.offset + chunk_b.chunk_size - 1,
            modified_chunk.is_last_chunk = false;

            changes.modified.emplace_back(std::move(modified_chunk));
        }
    }

    // create last chunk
    size_t last_index = changes.modified.size();
    if (last_index > 0)
        changes.modified[last_index - 1].is_last_chunk = true;

    // for new chunks add the start and end index of current snap
    for (; i < file_curr_snap.chunks.size(); ++i)
    {
        const ChunkInfo &chunk = file_curr_snap.chunks[i];
        AddChunkPayload new_chunk{.filename = file_curr_snap.filename,
                                  .new_start_index = chunk.offset,
                                  .new_end_index = chunk.offset + chunk.chunk_size - 1,
                                  .is_last_chunk = false};

        changes.added.emplace_back(std::move(new_chunk));
    }

    // create last chunk
    last_index = changes.added.size();
    if (last_index > 0)
        changes.added[last_index-1].is_last_chunk = true;

    // for removed chunks
    last_index = SIZE_MAX;
    bool is_removed = false;
    for (; i < file_prev_snap.chunks.size(); ++i)
    {
        last_index = std::min(static_cast<size_t>(last_index), file_prev_snap.chunks[i].offset);
        is_removed = true;
    }

    if (is_removed)
        changes.removed.emplace_back(TruncateFilePayload{
            .filename = file_prev_snap.filename,
            .last_index = static_cast<size_t>(last_index)});

    return changes;
}

// will save the snapshots as json in the file
void SnapshotManager::save_snapshot(const DirSnapshot &snaps)
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

    std::ofstream file(snap_file_path, std::ios::out);

    if (!file)
        throw std::runtime_error(std::format("failed to save snapshot to {}", snap_file_path.string()));

    file << j.dump(4);
    file.close();
}

// will load the saved file snaps from json
DirSnapshot SnapshotManager::load_snapshot()
{
    std::ifstream file(snap_file_path);

    DirSnapshot snapshots;

    if (!file.is_open())
    {
        std::cerr << "snap file file does not exist, returning default values" << std::endl;
        return snapshots;
    }
    if (fs::file_size(snap_file_path) == 0)
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