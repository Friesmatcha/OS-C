#include "core/PathResolver.hpp"

#include <algorithm>
#include <sstream>

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string part;
    while (std::getline(stream, part, '/')) {
        if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
    }
    return parts;
}

std::optional<int> findChildNodeId(const FsState& state, int parentId, const std::string& name) {
    for (const FsNode& node : state.nodes) {
        if (!node.deleted && node.parentId.has_value() && *node.parentId == parentId && node.name == name) {
            return node.id;
        }
    }
    return std::nullopt;
}

std::optional<int> resolvePath(const FsState& state, int currentDirectoryId, const std::string& path) {
    int nodeId = (!path.empty() && path.front() == '/') ? 1 : currentDirectoryId;
    for (const std::string& part : splitPath(path)) {
        if (part == "..") {
            auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) {
                return node.id == nodeId && !node.deleted;
            });
            if (it != state.nodes.end() && it->parentId.has_value()) {
                nodeId = *it->parentId;
            }
            continue;
        }
        std::optional<int> child = findChildNodeId(state, nodeId, part);
        if (!child.has_value()) {
            return std::nullopt;
        }
        nodeId = *child;
    }
    return nodeId;
}

std::string absolutePathForNode(const FsState& state, int nodeId) {
    if (nodeId == 1) {
        return "/";
    }
    std::vector<std::string> names;
    int cursor = nodeId;
    while (cursor != 1) {
        auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) {
            return node.id == cursor && !node.deleted;
        });
        if (it == state.nodes.end()) {
            return "/";
        }
        names.push_back(it->name);
        cursor = it->parentId.value_or(1);
    }
    std::string result;
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        result += "/" + *it;
    }
    return result.empty() ? "/" : result;
}

std::pair<std::optional<int>, std::string> resolveParentAndName(const FsState& state, int currentDirectoryId, const std::string& path) {
    std::vector<std::string> parts = splitPath(path);
    if (parts.empty() || parts.back() == "..") {
        return {std::nullopt, ""};
    }
    std::string name = parts.back();
    parts.pop_back();
    std::string parentPath = (!path.empty() && path.front() == '/') ? "/" : ".";
    for (const std::string& part : parts) {
        parentPath += (parentPath == "/" ? "" : "/") + part;
    }
    return {resolvePath(state, currentDirectoryId, parentPath), name};
}

