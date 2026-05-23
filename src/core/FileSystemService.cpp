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
