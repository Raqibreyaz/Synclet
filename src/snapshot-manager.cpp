#include "../include/snapshot-manager.hpp"

SnapshotManager::SnapshotManager(const std::string &data_dir_path, const std::string &snap_file_path) : data_dir_path(data_dir_path), snap_file_path(snap_file_path) {}

std::string SnapshotManager::create_hash(const std::vector<char> &data)
{
    if(data.empty())
    return "";

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
FileSnapshot SnapshotManager::createSnapshot(const std::string &file_path)
{
    const uint64_t file_size = fs::file_size(file_path);

    const time_t last_write_time = to_unix_timestamp(fs::last_write_time(file_path));

    // create a snapshot of the file
    FileSnapshot snapshot(extract_filename_from_path(file_path), file_size, last_write_time, {});

    if (file_size == 0)
        return snapshot;

    // open the file
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
        throw std::runtime_error(std::format("{} {}", std::string("failed to open file"), file_path));

    // min window size: 32
    // max window size: 128
    // dividing file_size with almost 1MB
    const int w_size = std::clamp<int>(file_size / 1000000, 32, 128);

    // min n: 2048
    const int n = std::max(2048, static_cast<int>(file_size / (512 * 1024)));

    const int modulo = 1e9 + 7;
    const int base = 256;

    /*
        window_size = w
        base = 256
        hash = C0*base^w-1 + C1*base^w-2 + ... + (Cw-1*base^0 or Cw-1)
        chunk_boundary -> hash % n == 0
    */

    // for computing hash
    long long hash = 0;
    size_t chunk_start = 0;

    // calculating highest power to avoid recomputation
    long long highest_power = 1;
    for (int i = 0; i < w_size - 1; i++)
        highest_power = (highest_power * base) % modulo;

    std::deque<char> window;
    std::vector<char> chunk;
    chunk.reserve(w_size * 2);

    const auto roll_hash = [&](char c)
    {
        hash = (1LL * hash * base + c) % modulo;
        window.push_back(c);
    };

    const size_t buffer_size = 1024 * 100;
    std::vector<char> buffer(buffer_size);

    // process a character each time
    int chunk_no = 0;

    while (file)
    {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();

        if (bytes_read == 0)
            break;

        for (size_t i = 0; i < static_cast<size_t>(bytes_read); i++)
        {
            char c = buffer[i];

            chunk.push_back(c);

            // when the window hit mark then remove oldest char
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
                auto &&chunk_object = ChunkInfo(chunk_start, chunk.size(), create_hash(chunk), chunk_no);

                snapshot.chunks[chunk_object.hash] = std::move(chunk_object);

                // move to next chunk
                chunk_start += chunk.size();
                chunk_no++;

                // clear this chunk data + window + hash
                hash = 0;
                chunk.clear();
                window.clear();
            }
        }
    };

    if (!chunk.empty())
    {
        auto &&chunk_object = ChunkInfo(chunk_start, chunk.size(), create_hash(chunk), chunk_no);
        snapshot.chunks[chunk_object.hash] = std::move(chunk_object);
    }

    return snapshot;
}

// Scan a directory and build a snapshot of all files and their chunks
std::pair<std::string, DirSnapshot> SnapshotManager::scan_directory()
{
    // for calculating snap_version
    std::stringstream ss;

    std::vector<std::pair<std::string, FileSnapshot>> sorted_snaps;

    // first store all the files snaps in vector
    for (const auto &entry : fs::directory_iterator(data_dir_path))
    {
        // file should be normal file , !socket, !directory
        if (!entry.is_regular_file())
            continue;

        // full path required for opening file
        FileSnapshot &&snapshot = createSnapshot(entry.path().string());

        sorted_snaps.push_back(std::make_pair(snapshot.filename, std::move(snapshot)));
    }

    // now sort the vector
    std::sort(sorted_snaps.begin(), sorted_snaps.end(), [](const auto &file_a, const auto &file_b)
              { return file_a.first < file_b.first; });

    //  adding data for snap version
    for (const auto &[filename, file_snap] : sorted_snaps)
    {
        ss << filename << "|" << file_snap.file_size;

        for (const auto &[_, chunk] : file_snap.chunks)
        {
            ss << "|" << chunk.offset << ":" << chunk.chunk_size << ":" << chunk.hash;
        }
        ss << "||";
    }

    std::string snap_str = ss.str();

    // trimming last redundant ||
    if (snap_str.size() >= 2 && snap_str.substr(snap_str.size() - 2) == "||")
        snap_str.erase(snap_str.size() - 2);

    return {
        create_hash(std::vector<char>(snap_str.begin(), snap_str.end())),
        DirSnapshot(sorted_snaps.begin(), sorted_snaps.end())};
}

// Compare two snapshots and return changed/added/deleted files
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
            changes.created_files.push_back(snap);
        }

        // check for file modification
        else if (snap.file_size != prev_snap->second.file_size || snap.mtime != prev_snap->second.mtime)
        {
            FileModification &&file_modification = get_file_modification(snap, prev_snap->second);

            if (!(file_modification.modified_chunks.empty()))
                changes.modified_files.push_back(std::move(file_modification));
        }
    }

    // check for file deletion
    for (const auto &[filename, snap] : prevSnapshot)
        if (currSnapshot.find(filename) == currSnapshot.end())
        {
            changes.removed_files.push_back(filename);
        }

    return changes;
}

// compare curr and prev snap of the file(using only hash) and get changes as modified, added, removed chunks
FileModification SnapshotManager::get_file_modification(const FileSnapshot &file_curr_snap, const FileSnapshot &file_prev_snap)
{
    FileModification changes{
        .filename = file_curr_snap.filename,
        .modified_chunks = {}};

    std::unordered_map<uint64_t, ChunkInfo> prev_offset_map;
    std::unordered_map<uint64_t, ChunkInfo> curr_offset_map;

    // create entries of offset for prev_snap
    for (auto &[_, chunk] : file_prev_snap.chunks)
        prev_offset_map[chunk.offset] = chunk;

    // create entries of offset for curr_snap
    for (auto &[_, chunk] : file_curr_snap.chunks)
        curr_offset_map[chunk.offset] = chunk;

    // check for removed chunks
    for (auto &[chunk_hash, chunk] : file_prev_snap.chunks)
    {
        const bool is_chunk_present = file_curr_snap.chunks.contains(chunk_hash);
        std::unordered_map<uint64_t, ChunkInfo>::iterator offset_chunk_it;

        // check if the offset is present when the chunk is not present
        if (!is_chunk_present)
            offset_chunk_it = curr_offset_map.find(chunk.offset);

        // when the chunk present in current snap or offset of the chunk is present and prev_chunks not contains the offset_chunk then skip it basically we are neglecting already present or modified changes as we want only removed chunks
        if (!is_chunk_present &&
            (offset_chunk_it == curr_offset_map.end() ||
             file_prev_snap.chunks.contains(offset_chunk_it->second.hash)))
        {
            ModifiedChunkPayload removed_chunk(ChunkType::REMOVE,
                                               file_prev_snap.filename,
                                               chunk.offset,
                                               chunk.chunk_size,
                                               0, false);

            changes.modified_chunks.push_back(std::move(removed_chunk));
        }
    }

    // check for added chunks
    for (auto &[chunk_hash, chunk] : file_curr_snap.chunks)
    {
        const bool is_chunk_present = file_prev_snap.chunks.contains(chunk_hash);
        std::unordered_map<uint64_t, ChunkInfo>::iterator offset_chunk_it;

        // check if the offset is present when the chunk is not present
        if (!is_chunk_present)
            offset_chunk_it = prev_offset_map.find(chunk.offset);

        // skip the chunk if it present previously or that corresponding offset contains a chunk which is not present currently as we are neglecting the chunks which were already present or are modified
        if (!is_chunk_present &&
            (offset_chunk_it == prev_offset_map.end() ||
             file_curr_snap.chunks.contains(offset_chunk_it->second.hash)))
        {
            ModifiedChunkPayload added_chunk(ChunkType::ADD,
                                             file_curr_snap.filename,
                                             chunk.offset,
                                             chunk.chunk_size,
                                             0, false);

            changes.modified_chunks.push_back(std::move(added_chunk));
        }
    }

    // check for modified chunks
    for (auto &[offset, chunk] : curr_offset_map)
    {
        auto prev_offset_it = prev_offset_map.find(offset);

        // when offset not found or data is not modified or the chunk is present at another offset or prev_chunk is present at another offset then skip
        if (prev_offset_it != prev_offset_map.end() &&
            prev_offset_it->second.hash != chunk.hash &&
            !(file_prev_snap.chunks.contains(chunk.hash)) &&
            !(file_curr_snap.chunks.contains(prev_offset_it->second.hash)))
        {
            ModifiedChunkPayload modified_chunk(ChunkType::MODIFY,
                                                file_curr_snap.filename,
                                                offset,
                                                chunk.chunk_size,
                                                prev_offset_it->second.chunk_size,
                                                false);

            changes.modified_chunks.push_back(std::move(modified_chunk));
        }
    }

    // for marking last chunk we need to sort according to offset
    const auto offset_sort = [](const auto &a, const auto &b) -> bool
    {
        return a.offset < b.offset;
    };
    std::sort(changes.modified_chunks.begin(),
              changes.modified_chunks.end(),
              offset_sort);

    //   now mark last chunk
    if (!changes.modified_chunks.empty())
        changes.modified_chunks.back().is_last_chunk = true;

    return changes;
}

// will save the snapshots as json in the file
void SnapshotManager::save_snapshot(const DirSnapshot &snaps)
{
    json j = json::object();
    json files = json::array();

    std::vector<std::pair<std::string, FileSnapshot>> sorted_snaps(snaps.begin(), snaps.end());

    std::sort(sorted_snaps.begin(), sorted_snaps.end(), [](const auto &snap_a, const auto &snap_b)
              { return snap_a.first < snap_b.first; });

    std::stringstream ss;

    for (const auto &[filename, snap] : sorted_snaps)
    {
        json file_json;
        file_json["filename"] = snap.filename;
        file_json["size"] = snap.file_size;
        file_json["mtime"] = snap.mtime;
        file_json["chunks"] = json::array();

        ss << filename << "|" << snap.file_size;

        for (const auto &[_, chunk] : snap.chunks)
        {
            file_json["chunks"]
                .push_back({{"offset", chunk.offset},
                            {"size", chunk.chunk_size},
                            {"hash", chunk.hash},
                            {"chunk_no", chunk.chunk_no}});

            ss << "|" << chunk.offset << ":" << chunk.chunk_size << ":" << chunk.hash;
        }

        ss << "||";

        files.push_back(file_json);
    }

    std::string snap_str = ss.str();

    // remove last redundant ||
    if (snap_str.size() >= 2 && snap_str.substr(snap_str.size() - 2) == "||")
        snap_str.erase(snap_str.size() - 2);

    j["version"] = create_hash(
        std::vector<char>(snap_str.begin(), snap_str.end()));
    j["files"] = std::move(files);

    std::ofstream file(snap_file_path, std::ios::out);

    if (!file)
        throw std::runtime_error(std::format("failed to save snapshot to {}", snap_file_path.string()));

    file << j.dump(4);
    file.close();
}

// will load the saved file snaps from json
std::pair<std::string, DirSnapshot> SnapshotManager::load_snapshot()
{
    std::ifstream file(snap_file_path);

    DirSnapshot snapshots;

    if (!file.is_open())
    {
        std::cerr << "snap file file does not exist, returning default values" << std::endl;
        return std::make_pair("", snapshots);
    }
    if (fs::file_size(snap_file_path) == 0)
    {
        std::cerr << "snap file is empty, returning default values" << std::endl;
        return std::make_pair("", snapshots);
    }

    json j;

    file >> j;

    for (const auto &file_json : j["files"])
    {
        const std::string filename = file_json["filename"].get<std::string>();
        const uint64_t file_size = file_json["size"].get<uint64_t>();
        const std::time_t mtime = file_json["mtime"].get<std::time_t>();

        std::unordered_map<std::string, ChunkInfo> chunks;

        for (const auto &chunk_json : file_json["chunks"])
        {
            ChunkInfo chunk;
            chunk.offset = chunk_json["offset"].get<size_t>();
            chunk.chunk_size = chunk_json["size"].get<size_t>();
            chunk.hash = chunk_json["hash"].get<std::string>();
            chunk.chunk_no = chunk_json["chunk_no"].get<int>();

            chunks[chunk.hash] = std::move(chunk);
        }

        snapshots[filename] = FileSnapshot(filename, file_size, mtime, chunks);
    }

    return std::make_pair(j["version"].get<std::string>(), snapshots);
}