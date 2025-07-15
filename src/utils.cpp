#include "../include/utils.hpp"

std::string sanitize_filename(const std::string &filename)
{
    const std::unordered_set<char> invalid_chars{
        '<', '>', ':', '"', '/', '\\', '|', '?', '*'};

    std::string sanitized;
    sanitized.reserve(filename.size());

    for (char ch : filename)
    {
        if (invalid_chars.count(ch) || ch <= 31) // control chars too
            sanitized.push_back('-');
        else
            sanitized.push_back(ch);
    }

    return sanitized;
}
std::string extract_filename_from_path(const std::string &dir_to_skip, const std::string &path)
{
    size_t part_to_skip_pos = path.find(dir_to_skip);

    // skipping the givem dir
    if (part_to_skip_pos != std::string::npos)
        part_to_skip_pos = part_to_skip_pos + dir_to_skip.size();
    else
        part_to_skip_pos = 0;

    // skipping ./
    if (path.starts_with("./") && part_to_skip_pos == 0)
        part_to_skip_pos += 2;

    // skipping leading slash
    if (path[part_to_skip_pos] == '/')
        part_to_skip_pos += 1;

    // returning final substring
    return path.substr(part_to_skip_pos);
}

std::string convert_to_binary_string(size_t n)
{
    uint32_t big_endian_n = htonl(static_cast<uint32_t>(n));

    return std::string(reinterpret_cast<const char *>(&big_endian_n), sizeof(big_endian_n));
}

std::time_t to_unix_timestamp(const std::filesystem::file_time_type &mtime)
{
    using namespace std::chrono;
    return system_clock::to_time_t(time_point_cast<system_clock::duration>(mtime - std::filesystem::file_time_type::clock::now() + system_clock::now()));
}

void print_progress_bar(const std::string &message, double progress, size_t bar_width)
{
    size_t filled = static_cast<size_t>(std::round(progress * bar_width));
    size_t empty = bar_width - filled;

    std::stringstream ss;
    for (size_t i = 0; i < filled; i++)
        ss << "\033[35m█\033[0m";
    for (size_t i = 0; i < empty; i++)
        ss << "░";

    std::clog << "\r"
              << message
              << " ["
              << ss.str()
              << "] " << std::fixed << std::setprecision(2)
              << (progress * 100) << "% completed" << std::flush;
}