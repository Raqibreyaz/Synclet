#include "file-pair-session.hpp"
#include "message-types.hpp"
#include "tcp-socket.hpp"
#include "messenger.hpp"
#include "chunk-handler.hpp"
#include "snapshot-manager.hpp"
#include <ranges>
#include <unordered_map>

class ReceiverMessageHandler
{
public:
    ReceiverMessageHandler(const std::string &working_dir, Messenger &messenger);
    void process_modified_chunk(const ModifiedChunkPayload &payload, DirSnapshot &snaps);
    void process_create_file(const FileCreateRemovePayload &payload, DirSnapshot &snaps);
    void process_create_file(const FilesCreatedPayload &payload, DirSnapshot &snaps);
    void process_delete_file(const FileCreateRemovePayload &payload, DirSnapshot &snaps);
    void process_delete_file(const FilesRemovedPayload &payload, DirSnapshot &snaps);
    void process_file_moved(const FileMovedPayload &payload, DirSnapshot &snaps);
    void process_create_dir(const DirCreateRemovePayload &payload);
    void process_create_dir(const DirsCreatedRemovedPayload &payload);
    void process_delete_dir(const DirCreateRemovePayload &payload, DirSnapshot &snaps);
    void process_delete_dir(const DirsCreatedRemovedPayload &payload, DirSnapshot &snaps);
    void process_dir_moved(const DirMovedPayload &payload, DirSnapshot &snaps);
    void process_file(const SendFilePayload &payload, DirSnapshot &snaps);
    void process_file_chunk(const SendChunkPayload &payload, DirSnapshot &snaps);
    void process_fetch_files(const std::vector<std::string> &files, DirSnapshot &snaps);
    void process_fetch_modified_chunks(const std::vector<FileModification> &modified_files, DirSnapshot &snaps);
    std::string process_request_snap_version();
    void process_request_peer_snap(DirSnapshot &peer_snaps);
    void process_request_peer_dir_list(std::vector<std::string> &dir_list);

private:
    std::string working_dir;
    Messenger &messenger;
    void process_get_files(const std::vector<std::string> &files, DirSnapshot &snaps);
};