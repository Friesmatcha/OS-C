#pragma once

#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class NodeType { File, Directory };
enum class OpenMode { Read, Write, ReadWrite };
enum class UserRole { Admin, User };
enum class UserStatus { Active, Disabled };

struct Permission {
    std::string value = "rwx------";
};

struct UserGroup {
    int id = 0;
    std::string name;
    std::time_t createdAt = 0;
};

struct User {
    int id = 0;
    std::string username;
    std::string passwordHash;
    int groupId = 0;
    UserRole role = UserRole::User;
    UserStatus status = UserStatus::Active;
    std::time_t createdAt = 0;
};

struct FsNode {
    int id = 0;
    std::optional<int> parentId;
    std::string name;
    NodeType type = NodeType::File;
    int ownerUserId = 0;
    int groupId = 0;
    Permission permissions;
    std::size_t sizeBytes = 0;
    std::time_t createdAt = 0;
    std::time_t updatedAt = 0;
    bool deleted = false;
};

struct DataBlock {
    int id = 0;
    int blockNo = 0;
    bool used = false;
    std::string content;
    std::time_t updatedAt = 0;
};

struct FileBlockIndex {
    int fileNodeId = 0;
    int logicalBlockNo = 0;
    int dataBlockId = 0;
};

struct OpenFileEntry {
    int fd = 0;
    int userId = 0;
    int fileNodeId = 0;
    OpenMode mode = OpenMode::Read;
    std::size_t offsetBytes = 0;
    std::time_t openedAt = 0;
};

struct OperationLog {
    int id = 0;
    std::optional<int> userId;
    std::string command;
    std::string result;
    bool success = false;
    std::time_t createdAt = 0;
};

struct FsState {
    std::vector<UserGroup> groups;
    std::vector<User> users;
    std::vector<FsNode> nodes;
    std::vector<DataBlock> dataBlocks;
    std::vector<FileBlockIndex> indexes;
    std::vector<OperationLog> logs;
    int nextGroupId = 1;
    int nextUserId = 1;
    int nextNodeId = 1;
    int nextBlockId = 1;
    int nextLogId = 1;
};

struct CommandResult {
    bool success = false;
    std::string message;
};

inline std::string hashPasswordForDevelopment(const std::string& password) {
    return "devhash:" + std::to_string(std::hash<std::string>{}(password));
}

inline FsState createDefaultState() {
    FsState state;
    const std::time_t now = std::time(nullptr);
    state.groups.push_back({1, "admin", now});
    state.groups.push_back({2, "users", now});
    state.users.push_back({1, "admin", hashPasswordForDevelopment("admin"), 1, UserRole::Admin, UserStatus::Active, now});
    state.users.push_back({2, "user1", hashPasswordForDevelopment("user1"), 2, UserRole::User, UserStatus::Active, now});
    state.users.push_back({3, "user2", hashPasswordForDevelopment("user2"), 2, UserRole::User, UserStatus::Active, now});
    state.nodes.push_back({1, std::nullopt, "/", NodeType::Directory, 1, 1, Permission{"rwxr-xr-x"}, 0, now, now, false});
    state.nextGroupId = 3;
    state.nextUserId = 4;
    state.nextNodeId = 2;
    return state;
}

