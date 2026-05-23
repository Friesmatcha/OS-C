#include "persistence/JsonRepository.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <utility>

JsonRepository::JsonRepository(std::string path)
    : path_(std::move(path)) {}

static std::string escapeJson(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '\\') {
            result += "\\\\";
        } else if (ch == '"') {
            result += "\\\"";
        } else if (ch == '\n') {
            result += "\\n";
        } else {
            result += ch;
        }
    }
    return result;
}

static std::string unescapeJson(std::string value) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[++i];
            result += next == 'n' ? '\n' : next;
        } else {
            result += value[i];
        }
    }
    return result;
}

static std::string stringField(const std::string& line, const std::string& key) {
    std::regex pattern("\"" + key + "\":\"((?:\\\\.|[^\"])*)\"");
    std::smatch match;
    return std::regex_search(line, match, pattern) ? unescapeJson(match[1].str()) : "";
}

static int intField(const std::string& line, const std::string& key) {
    std::regex pattern("\"" + key + "\":(-?[0-9]+)");
    std::smatch match;
    return std::regex_search(line, match, pattern) ? std::stoi(match[1].str()) : 0;
}

static bool boolField(const std::string& line, const std::string& key) {
    std::regex pattern("\"" + key + "\":(true|false)");
    std::smatch match;
    return std::regex_search(line, match, pattern) && match[1].str() == "true";
}

CommandResult JsonRepository::save(const FsState& state) const {
    std::filesystem::path target(path_);
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path());
    }
    std::ofstream out(path_);
    if (!out) {
        return {false, "Storage error"};
    }

    out << "{\n";
    out << "  \"nextGroupId\":" << state.nextGroupId << ",\n";
    out << "  \"nextUserId\":" << state.nextUserId << ",\n";
    out << "  \"nextNodeId\":" << state.nextNodeId << ",\n";
    out << "  \"nextBlockId\":" << state.nextBlockId << ",\n";
    out << "  \"nextLogId\":" << state.nextLogId << ",\n";

    out << "  \"groups\": [\n";
    for (std::size_t i = 0; i < state.groups.size(); ++i) {
        const auto& group = state.groups[i];
        out << "    {\"id\":" << group.id << ",\"name\":\"" << escapeJson(group.name)
            << "\",\"createdAt\":" << group.createdAt << "}" << (i + 1 == state.groups.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";

    out << "  \"users\": [\n";
    for (std::size_t i = 0; i < state.users.size(); ++i) {
        const auto& user = state.users[i];
        out << "    {\"id\":" << user.id << ",\"username\":\"" << escapeJson(user.username)
            << "\",\"passwordHash\":\"" << escapeJson(user.passwordHash)
            << "\",\"groupId\":" << user.groupId
            << ",\"role\":\"" << (user.role == UserRole::Admin ? "admin" : "user")
            << "\",\"status\":\"" << (user.status == UserStatus::Active ? "active" : "disabled")
            << "\",\"createdAt\":" << user.createdAt << "}" << (i + 1 == state.users.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";

    out << "  \"nodes\": [\n";
    for (std::size_t i = 0; i < state.nodes.size(); ++i) {
        const auto& node = state.nodes[i];
        out << "    {\"id\":" << node.id << ",\"parentId\":" << node.parentId.value_or(0)
            << ",\"name\":\"" << escapeJson(node.name)
            << "\",\"type\":\"" << (node.type == NodeType::Directory ? "directory" : "file")
            << "\",\"ownerUserId\":" << node.ownerUserId
            << ",\"groupId\":" << node.groupId
            << ",\"permissions\":\"" << node.permissions.value
            << "\",\"sizeBytes\":" << node.sizeBytes
            << ",\"createdAt\":" << node.createdAt
            << ",\"updatedAt\":" << node.updatedAt
            << ",\"deleted\":" << (node.deleted ? "true" : "false") << "}" << (i + 1 == state.nodes.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";

    out << "  \"dataBlocks\": [\n";
    for (std::size_t i = 0; i < state.dataBlocks.size(); ++i) {
        const auto& block = state.dataBlocks[i];
        out << "    {\"id\":" << block.id << ",\"blockNo\":" << block.blockNo
            << ",\"used\":" << (block.used ? "true" : "false")
            << ",\"content\":\"" << escapeJson(block.content)
            << "\",\"updatedAt\":" << block.updatedAt << "}" << (i + 1 == state.dataBlocks.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";

    out << "  \"indexes\": [\n";
    for (std::size_t i = 0; i < state.indexes.size(); ++i) {
        const auto& index = state.indexes[i];
        out << "    {\"fileNodeId\":" << index.fileNodeId << ",\"logicalBlockNo\":" << index.logicalBlockNo
            << ",\"dataBlockId\":" << index.dataBlockId << "}" << (i + 1 == state.indexes.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";

    out << "  \"logs\": [\n";
    for (std::size_t i = 0; i < state.logs.size(); ++i) {
        const auto& log = state.logs[i];
        out << "    {\"id\":" << log.id << ",\"userId\":" << log.userId.value_or(0)
            << ",\"command\":\"" << escapeJson(log.command)
            << "\",\"result\":\"" << escapeJson(log.result)
            << "\",\"success\":" << (log.success ? "true" : "false")
            << ",\"createdAt\":" << log.createdAt << "}" << (i + 1 == state.logs.size() ? "" : ",") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return {true, "Saved"};
}

std::optional<FsState> JsonRepository::load() const {
    std::ifstream in(path_);
    if (!in) {
        return std::nullopt;
    }
    FsState state;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"nextGroupId\"") != std::string::npos) state.nextGroupId = intField(line, "nextGroupId");
        else if (line.find("\"nextUserId\"") != std::string::npos) state.nextUserId = intField(line, "nextUserId");
        else if (line.find("\"nextNodeId\"") != std::string::npos) state.nextNodeId = intField(line, "nextNodeId");
        else if (line.find("\"nextBlockId\"") != std::string::npos) state.nextBlockId = intField(line, "nextBlockId");
        else if (line.find("\"nextLogId\"") != std::string::npos) state.nextLogId = intField(line, "nextLogId");
        else if (line.find("\"groups\"") != std::string::npos) section = "groups";
        else if (line.find("\"users\"") != std::string::npos) section = "users";
        else if (line.find("\"nodes\"") != std::string::npos) section = "nodes";
        else if (line.find("\"dataBlocks\"") != std::string::npos) section = "dataBlocks";
        else if (line.find("\"indexes\"") != std::string::npos) section = "indexes";
        else if (line.find("\"logs\"") != std::string::npos) section = "logs";
        else if (line.find("]") != std::string::npos) section.clear();
        else if (line.find("{") != std::string::npos && !section.empty()) {
            if (section == "groups") {
                state.groups.push_back({intField(line, "id"), stringField(line, "name"), static_cast<std::time_t>(intField(line, "createdAt"))});
            } else if (section == "users") {
                User user;
                user.id = intField(line, "id");
                user.username = stringField(line, "username");
                user.passwordHash = stringField(line, "passwordHash");
                user.groupId = intField(line, "groupId");
                user.role = stringField(line, "role") == "admin" ? UserRole::Admin : UserRole::User;
                user.status = stringField(line, "status") == "disabled" ? UserStatus::Disabled : UserStatus::Active;
                user.createdAt = intField(line, "createdAt");
                state.users.push_back(user);
            } else if (section == "nodes") {
                FsNode node;
                node.id = intField(line, "id");
                int parentId = intField(line, "parentId");
                node.parentId = parentId == 0 ? std::optional<int>{} : std::optional<int>{parentId};
                node.name = stringField(line, "name");
                node.type = stringField(line, "type") == "directory" ? NodeType::Directory : NodeType::File;
                node.ownerUserId = intField(line, "ownerUserId");
                node.groupId = intField(line, "groupId");
                node.permissions.value = stringField(line, "permissions");
                node.sizeBytes = static_cast<std::size_t>(intField(line, "sizeBytes"));
                node.createdAt = intField(line, "createdAt");
                node.updatedAt = intField(line, "updatedAt");
                node.deleted = boolField(line, "deleted");
                state.nodes.push_back(node);
            } else if (section == "dataBlocks") {
                DataBlock block;
                block.id = intField(line, "id");
                block.blockNo = intField(line, "blockNo");
                block.used = boolField(line, "used");
                block.content = stringField(line, "content");
                block.updatedAt = intField(line, "updatedAt");
                state.dataBlocks.push_back(block);
            } else if (section == "indexes") {
                state.indexes.push_back({intField(line, "fileNodeId"), intField(line, "logicalBlockNo"), intField(line, "dataBlockId")});
            } else if (section == "logs") {
                OperationLog log;
                log.id = intField(line, "id");
                int userId = intField(line, "userId");
                log.userId = userId == 0 ? std::optional<int>{} : std::optional<int>{userId};
                log.command = stringField(line, "command");
                log.result = stringField(line, "result");
                log.success = boolField(line, "success");
                log.createdAt = intField(line, "createdAt");
                state.logs.push_back(log);
            }
        }
    }
    return state;
}

