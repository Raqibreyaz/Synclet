#include "file-pair-session.hpp"
#include "message-types.hpp"
#include "tcp-socket.hpp"
#include "messenger.hpp"
#include "chunk-handler.hpp"
#include <unordered_map>

class ServerMessageHandler
{
public:
    ServerMessageHandler(const std::string &working_dir, TcpConnection &client,Messenger& messenger);
    void process_modified_chunk(const ModifiedChunkPayload &payload);
    void process_add_remove_chunk(const AddRemoveChunkPayload &payload, const bool is_removed = false);
    void process_create_file(const FileCreateRemovePayload &payload);
    void process_create_file(const FilesCreatedPayload &payload);
    void process_delete_file(const FileCreateRemovePayload &payload);
    void process_delete_file(const FilesRemovedPayload &payload);
    void process_file_rename(const FileRenamePayload &payload);
    void process_file_chunk(const SendChunkPayload &payload);
    void process_fetch_files(const std::vector<std::string>& files);
    // void process_fetch_modified_chunks(const std::vector& chunks);

private:
    std::string working_dir;
    TcpConnection &client;
    Messenger& messenger;
    std::unique_ptr<FilePairSession> active_session;
    void get_or_create_session(const std::string &filename,const bool append_to_original = false);
};