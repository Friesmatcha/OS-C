#pragma once

#include "domain/Types.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

std::vector<std::string> splitPath(const std::string& path);
std::optional<int> findChildNodeId(const FsState& state, int parentId, const std::string& name);
std::optional<int> resolvePath(const FsState& state, int currentDirectoryId, const std::string& path);
std::string absolutePathForNode(const FsState& state, int nodeId);
std::pair<std::optional<int>, std::string> resolveParentAndName(const FsState& state, int currentDirectoryId, const std::string& path);

