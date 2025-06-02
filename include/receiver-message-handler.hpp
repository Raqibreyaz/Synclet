#include "file-pair-session.hpp"
#include "message-types.hpp"
#include "tcp-socket.hpp"
#include "messenger.hpp"
#include "chunk-handler.hpp"
#include "snapshot-manager.hpp"
#include <unordered_map>

class ReceiverMessageHandler
{
public:
    ReceiverMessageHandler(const std::string &working_dir, Messenger &messenger);
    void process_modified_chunk(const ModifiedChunkPayload &payload);
    void process_create_file(const FileCreateRemovePayload &payload);
    void process_create_file(const FilesCreatedPayload &payload);
    void process_delete_file(const FileCreateRemovePayload &payload);
    void process_delete_file(const FilesRemovedPayload &payload);
    void process_file_rename(const FileRenamePayload &payload);
    void process_file_chunk(const SendChunkPayload &payload);
    void process_fetch_files(const std::vector<std::string> &files);
    void process_fetch_modified_chunks(const std::vector<FileModification> &modified_files);
    std::string process_request_snap_version();
    std::vector<FileSnapshot> process_request_peer_snap();

private:
    std::string working_dir;
    Messenger &messenger;
    void process_get_files(const std::vector<std::string>& files);
};