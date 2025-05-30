#include "file-pair-session.hpp"
#include "message-types.hpp"
#include "tcp-socket.hpp"
#include <unordered_map>

class ServerMessageHandler
{
public:
    ServerMessageHandler(const std::string &working_dir, TcpConnection &client);
    void process_modified_chunk(const ModifiedChunkPayload &payload);
    void process_add_remove_chunk(const AddRemoveChunkPayload &payload, const bool is_removed = false);
    void process_create_file(const FileCreateRemovePayload &payload);
    void process_delete_file(const FileCreateRemovePayload &payload);
    void process_file_rename(const FileRenamePayload &payload);

private:
    std::string working_dir;
    TcpConnection &client;
    std::unique_ptr<FilePairSession> active_session;
    void get_or_create_session(const std::string &filename);
};