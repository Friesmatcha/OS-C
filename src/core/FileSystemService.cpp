#include "core/FileSystemService.hpp"
#include "core/PathResolver.hpp"

#include <algorithm>
#include <utility>

FileSystemService::FileSystemService(FsState state)
    : state_(std::move(state)) {}

const User* FileSystemService::findUserByName(const std::string& username) const {
    auto it = std::find_if(state_.users.begin(), state_.users.end(), [&](const User& user) {
        return user.username == username;
    });
    return it == state_.users.end() ? nullptr : &*it;
}

CommandResult FileSystemService::login(const std::string& username, const std::string& password) {
    const User* user = findUserByName(username);
    if (user == nullptr || user->passwordHash != hashPasswordForDevelopment(password) || user->status != UserStatus::Active) {
        return {false, "Invalid username or password"};
    }
    currentUserId_ = user->id;
    currentDirectoryId_ = 1;
    openFiles_.clear();
    nextFd_ = 3;
    return {true, "Logged in as " + username};
}

CommandResult FileSystemService::logout() {
    currentUserId_.reset();
    currentDirectoryId_ = 1;
    openFiles_.clear();
    nextFd_ = 3;
    return {true, "Logged out"};
}

CommandResult FileSystemService::pwd() const {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    return {true, absolutePathForNode(state_, currentDirectoryId_)};
}

std::string FileSystemService::currentUsername() const {
    if (!currentUserId_.has_value()) {
        return "";
    }
    auto it = std::find_if(state_.users.begin(), state_.users.end(), [&](const User& user) {
        return user.id == *currentUserId_;
    });
    return it == state_.users.end() ? "" : it->username;
}


const FsState& FileSystemService::state() const {
    return state_;
}

CommandResult FileSystemService::mkdir(const std::string& path) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    auto [parentId, name] = resolveParentAndName(state_, currentDirectoryId_, path);
    if (!parentId.has_value()) {
        return {false, "Not found"};
    }
    if (findChildNodeId(state_, *parentId, name).has_value()) {
        return {false, "Already exists"};
    }
    const std::time_t now = std::time(nullptr);
    state_.nodes.push_back({state_.nextNodeId++, *parentId, name, NodeType::Directory, *currentUserId_, 1, Permission{"rwxr-xr-x"}, 0, now, now, false});
    return {true, "Directory created"};
}

CommandResult FileSystemService::cd(const std::string& path) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    auto it = std::find_if(state_.nodes.begin(), state_.nodes.end(), [&](const FsNode& node) {
        return node.id == *nodeId && !node.deleted;
    });
    if (it == state_.nodes.end() || it->type != NodeType::Directory) {
        return {false, "Not a directory"};
    }
    currentDirectoryId_ = *nodeId;
    return {true, absolutePathForNode(state_, currentDirectoryId_)};
}

CommandResult FileSystemService::ls(const std::string& path) const {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    std::string output;
    for (const FsNode& node : state_.nodes) {
        if (!node.deleted && node.parentId.has_value() && *node.parentId == *nodeId) {
            output += node.name + " " + (node.type == NodeType::Directory ? "directory" : "file") + "\n";
        }
    }
    return {true, output};
}

static bool isValidPermission(const std::string& mode) {
    if (mode.size() != 9) {
        return false;
    }
    const char expected[9] = {'r','w','x','r','w','x','r','w','x'};
    for (std::size_t i = 0; i < mode.size(); ++i) {
        if (mode[i] != expected[i] && mode[i] != '-') {
            return false;
        }
    }
    return true;
}

static FsNode* findNode(FsState& state, int id) {
    auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) { return node.id == id && !node.deleted; });
    return it == state.nodes.end() ? nullptr : &*it;
}

static const FsNode* findNodeConst(const FsState& state, int id) {
    auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) { return node.id == id && !node.deleted; });
    return it == state.nodes.end() ? nullptr : &*it;
}

CommandResult FileSystemService::create(const std::string& path) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    auto [parentId, name] = resolveParentAndName(state_, currentDirectoryId_, path);
    if (!parentId.has_value()) {
        return {false, "Not found"};
    }
    if (findChildNodeId(state_, *parentId, name).has_value()) {
        return {false, "Already exists"};
    }
    const std::time_t now = std::time(nullptr);
    state_.nodes.push_back({state_.nextNodeId++, *parentId, name, NodeType::File, *currentUserId_, 1, Permission{"rw-r--r--"}, 0, now, now, false});
    return {true, "File created"};
}

CommandResult FileSystemService::stat(const std::string& path) const {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    const FsNode* node = findNodeConst(state_, *nodeId);
    if (node == nullptr) {
        return {false, "Not found"};
    }
    return {true, node->name + " " + (node->type == NodeType::Directory ? "directory " : "file ") + std::to_string(node->sizeBytes) + " " + node->permissions.value};
}

CommandResult FileSystemService::chmod(const std::string& path, const std::string& mode) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    if (!isValidPermission(mode)) {
        return {false, "Invalid arguments"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    FsNode* node = findNode(state_, *nodeId);
    if (node == nullptr) {
        return {false, "Not found"};
    }
    node->permissions.value = mode;
    node->updatedAt = std::time(nullptr);
    return {true, "Permissions changed"};
}

CommandResult FileSystemService::rm(const std::string& path) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    FsNode* node = findNode(state_, *nodeId);
    if (node == nullptr) {
        return {false, "Not found"};
    }
    if (node->type != NodeType::File) {
        return {false, "Not a file"};
    }
    node->deleted = true;
    return {true, "File removed"};
}

static std::optional<OpenMode> parseOpenMode(const std::string& mode) {
    if (mode == "r") return OpenMode::Read;
    if (mode == "w") return OpenMode::Write;
    if (mode == "rw") return OpenMode::ReadWrite;
    return std::nullopt;
}

static bool isWriteMode(OpenMode mode) {
    return mode == OpenMode::Write || mode == OpenMode::ReadWrite;
}

static bool isReadMode(OpenMode mode) {
    return mode == OpenMode::Read || mode == OpenMode::ReadWrite;
}

static bool hasLockConflict(const std::vector<OpenFileEntry>& openFiles, int nodeId, OpenMode requested) {
    for (const OpenFileEntry& entry : openFiles) {
        if (entry.fileNodeId != nodeId) {
            continue;
        }
        if (isWriteMode(entry.mode) || isWriteMode(requested)) {
            return true;
        }
    }
    return false;
}

static std::string readFileContent(const FsState& state, int nodeId) {
    std::vector<FileBlockIndex> indexes;
    for (const FileBlockIndex& index : state.indexes) {
        if (index.fileNodeId == nodeId) {
            indexes.push_back(index);
        }
    }
    std::sort(indexes.begin(), indexes.end(), [](const auto& left, const auto& right) {
        return left.logicalBlockNo < right.logicalBlockNo;
    });
    std::string content;
    for (const FileBlockIndex& index : indexes) {
        auto block = std::find_if(state.dataBlocks.begin(), state.dataBlocks.end(), [&](const DataBlock& item) {
            return item.id == index.dataBlockId && item.used;
        });
        if (block != state.dataBlocks.end()) {
            content += block->content;
        }
    }
    const FsNode* node = findNodeConst(state, nodeId);
    if (node != nullptr && content.size() > node->sizeBytes) {
        content.resize(node->sizeBytes);
    }
    return content;
}

static void writeFileContent(FsState& state, int nodeId, const std::string& content) {
    for (DataBlock& block : state.dataBlocks) {
        for (const FileBlockIndex& index : state.indexes) {
            if (index.fileNodeId == nodeId && index.dataBlockId == block.id) {
                block.used = false;
                block.content.clear();
            }
        }
    }
    state.indexes.erase(std::remove_if(state.indexes.begin(), state.indexes.end(), [&](const FileBlockIndex& index) {
        return index.fileNodeId == nodeId;
    }), state.indexes.end());
    constexpr std::size_t blockSize = 512;
    int logical = 0;
    for (std::size_t offset = 0; offset < content.size(); offset += blockSize) {
        DataBlock block;
        block.id = state.nextBlockId++;
        block.blockNo = block.id;
        block.used = true;
        block.content = content.substr(offset, blockSize);
        block.updatedAt = std::time(nullptr);
        state.dataBlocks.push_back(block);
        state.indexes.push_back({nodeId, logical++, block.id});
    }
    FsNode* node = findNode(state, nodeId);
    if (node != nullptr) {
        node->sizeBytes = content.size();
        node->updatedAt = std::time(nullptr);
    }
}

CommandResult FileSystemService::open(const std::string& path, const std::string& mode) {
    if (!currentUserId_.has_value()) return {false, "Not logged in"};
    std::optional<OpenMode> parsed = parseOpenMode(mode);
    if (!parsed.has_value()) return {false, "Invalid arguments"};
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) return {false, "Not found"};
    FsNode* node = findNode(state_, *nodeId);
    if (node == nullptr || node->type != NodeType::File) return {false, "Not a file"};
    if (hasLockConflict(openFiles_, node->id, *parsed)) return {false, "File is locked"};
    int fd = nextFd_++;
    openFiles_.push_back({fd, *currentUserId_, node->id, *parsed, 0, std::time(nullptr)});
    return {true, "fd " + std::to_string(fd)};
}

CommandResult FileSystemService::close(int fd) {
    auto it = std::find_if(openFiles_.begin(), openFiles_.end(), [&](const OpenFileEntry& entry) { return entry.fd == fd; });
    if (it == openFiles_.end()) return {false, "Invalid fd"};
    openFiles_.erase(it);
    return {true, "Closed"};
}

CommandResult FileSystemService::read(int fd, std::size_t size) {
    auto it = std::find_if(openFiles_.begin(), openFiles_.end(), [&](const OpenFileEntry& entry) { return entry.fd == fd; });
    if (it == openFiles_.end()) return {false, "Invalid fd"};
    if (!isReadMode(it->mode)) return {false, "Permission denied"};
    std::string content = readFileContent(state_, it->fileNodeId);
    std::string output = it->offsetBytes >= content.size() ? "" : content.substr(it->offsetBytes, size);
    it->offsetBytes += output.size();
    return {true, output};
}

CommandResult FileSystemService::write(int fd, const std::string& content) {
    auto it = std::find_if(openFiles_.begin(), openFiles_.end(), [&](const OpenFileEntry& entry) { return entry.fd == fd; });
    if (it == openFiles_.end()) return {false, "Invalid fd"};
    if (!isWriteMode(it->mode)) return {false, "Permission denied"};
    std::string existing = readFileContent(state_, it->fileNodeId);
    if (it->offsetBytes > existing.size()) {
        existing.resize(it->offsetBytes, '\0');
    }
    existing.replace(it->offsetBytes, content.size(), content);
    writeFileContent(state_, it->fileNodeId, existing);
    it->offsetBytes += content.size();
    return {true, "Wrote " + std::to_string(content.size()) + " bytes"};
}

CommandResult FileSystemService::registerUser(const std::string& username, const std::string& password) {
    if (findUserByName(username) != nullptr) {
        return {false, "Already exists"};
    }
    state_.users.push_back({state_.nextUserId++, username, hashPasswordForDevelopment(password), 2, UserRole::User, UserStatus::Active, std::time(nullptr)});
    return {true, "User registered"};
}

CommandResult FileSystemService::users() const {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::string output;
    for (const User& user : state_.users) {
        output += user.username + " " + (user.role == UserRole::Admin ? "admin" : "user") + " " + (user.status == UserStatus::Active ? "active" : "disabled") + "\n";
    }
    return {true, output};
}

CommandResult FileSystemService::rmdir(const std::string& path) {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    std::optional<int> nodeId = resolvePath(state_, currentDirectoryId_, path);
    if (!nodeId.has_value()) {
        return {false, "Not found"};
    }
    FsNode* node = findNode(state_, *nodeId);
    if (node == nullptr || node->type != NodeType::Directory) {
        return {false, "Not a directory"};
    }
    if (node->id == 1) {
        return {false, "Permission denied"};
    }
    for (const FsNode& child : state_.nodes) {
        if (!child.deleted && child.parentId.has_value() && *child.parentId == node->id) {
            return {false, "Directory not empty"};
        }
    }
    node->deleted = true;
    return {true, "Directory removed"};
}

void FileSystemService::recordLog(const std::string& rawCommand, const CommandResult& result) {
    state_.logs.push_back({state_.nextLogId++, currentUserId_, rawCommand, result.message, result.success, std::time(nullptr)});
}
