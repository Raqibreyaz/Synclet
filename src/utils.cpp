#include "../include/utils.hpp"

std::string extract_filename_from_path(const std::string &path)
{
    if (path.empty())
        return "";

    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos)
        last_slash = -1;

    return path.substr(last_slash + 1);
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
