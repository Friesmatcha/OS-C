# CLI JSON File System MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++17 command-line multi-user simulated file system with JSON persistence, Linux-like permissions, file descriptors, and read/write protection.

**Architecture:** The implementation is a small layered C++ project. Domain types are plain data structures, core services implement all file-system behavior, persistence owns JSON load/save, and the CLI only parses commands and calls the service.

**Tech Stack:** C++17, CMake, a header-only minimal test harness, standard library filesystem/streams, and a local JSON encoder/decoder tailored to the project state file.

---

## File Structure

- Create `CMakeLists.txt`: builds `osc_fs` and `osc_fs_tests`.
- Create `src/domain/Types.hpp`: enums, structs, permission helpers, command result types, and full in-memory state.
- Create `src/core/PathResolver.hpp` and `src/core/PathResolver.cpp`: normalizes and resolves paths against current directory and permission rules.
- Create `src/core/FileSystemService.hpp` and `src/core/FileSystemService.cpp`: user/session operations, filesystem commands, open file table, storage block logic, and persistence coordination.
- Create `src/persistence/JsonRepository.hpp` and `src/persistence/JsonRepository.cpp`: load/save full state from `data/fs_state.json`.
- Create `src/cli/CommandParser.hpp` and `src/cli/CommandParser.cpp`: parse raw CLI command lines into command name and arguments.
- Create `src/cli/main.cpp`: interactive shell.
- Create `tests/TestHarness.hpp`: small assertion macros and test registration.
- Create `tests/FileSystemTests.cpp`: behavior tests for initialization, paths, permissions, file operations, protection, and JSON recovery.
- Modify `.gitignore`: ignore build outputs and runtime `data/fs_state.json`.

## Task 1: Project Scaffold and Test Harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`
- Create: `tests/TestHarness.hpp`
- Create: `tests/FileSystemTests.cpp`

- [ ] **Step 1: Write the failing smoke test**

Create `tests/TestHarness.hpp`:

```cpp
#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& testCases() {
    static std::vector<TestCase> cases;
    return cases;
}

struct TestRegistrar {
    TestRegistrar(std::string name, std::function<void()> body) {
        testCases().push_back({std::move(name), std::move(body)});
    }
};

#define TEST_CASE(name) \
    static void name(); \
    static TestRegistrar registrar_##name(#name, name); \
    static void name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(std::string("ASSERT_TRUE failed: ") + #expr); \
        } \
    } while (false)

#define ASSERT_EQ(actual, expected) \
    do { \
        auto actualValue = (actual); \
        auto expectedValue = (expected); \
        if (!(actualValue == expectedValue)) { \
            throw std::runtime_error(std::string("ASSERT_EQ failed: ") + #actual + " != " + #expected); \
        } \
    } while (false)
```

Create `tests/FileSystemTests.cpp`:

```cpp
#include "TestHarness.hpp"

TEST_CASE(smoke_test_runner_executes_tests) {
    ASSERT_TRUE(true);
    ASSERT_EQ(1 + 1, 2);
}

int main() {
    int failed = 0;
    for (const auto& test : testCases()) {
        try {
            test.body();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }
    if (failed != 0) {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cout << testCases().size() << " test(s) passed\n";
    return 0;
}
```

- [ ] **Step 2: Run test build to verify it fails**

Run: `cmake -S . -B build && cmake --build build && ./build/osc_fs_tests`

Expected: FAIL during CMake configure because `CMakeLists.txt` does not exist.

- [ ] **Step 3: Add minimal build files**

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(osc_fs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(osc_fs_tests
    tests/FileSystemTests.cpp
)

target_include_directories(osc_fs_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
```

Create `.gitignore`:

```gitignore
build/
data/fs_state.json
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake -S . -B build && cmake --build build && ./build/osc_fs_tests`

Expected: PASS with output containing `[PASS] smoke_test_runner_executes_tests`.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt .gitignore tests/TestHarness.hpp tests/FileSystemTests.cpp
git commit -m "test: add C++ test harness"
```

## Task 2: Domain Types and Initial State

**Files:**
- Create: `src/domain/Types.hpp`
- Modify: `tests/FileSystemTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing initial-state test**

Append to `tests/FileSystemTests.cpp` above `main()`:

```cpp
#include "domain/Types.hpp"

TEST_CASE(default_state_contains_three_users_and_root) {
    FsState state = createDefaultState();

    ASSERT_EQ(state.users.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(state.groups.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(state.nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(state.nodes.front().name, std::string("/"));
    ASSERT_EQ(state.nodes.front().permissions.value, std::string("rwxr-xr-x"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time with `domain/Types.hpp` not found.

- [ ] **Step 3: Implement domain types and default state**

Create `src/domain/Types.hpp`:

```cpp
#pragma once

#include <ctime>
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with two passing tests.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt src/domain/Types.hpp tests/FileSystemTests.cpp
git commit -m "feat: add filesystem domain state"
```

## Task 3: Session, Login, and Basic Service

**Files:**
- Create: `src/core/FileSystemService.hpp`
- Create: `src/core/FileSystemService.cpp`
- Modify: `tests/FileSystemTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing login tests**

Append to `tests/FileSystemTests.cpp`:

```cpp
#include "core/FileSystemService.hpp"

TEST_CASE(login_accepts_default_admin_and_sets_session) {
    FileSystemService service(createDefaultState());

    CommandResult result = service.login("admin", "admin");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.message, std::string("Logged in as admin"));
    ASSERT_EQ(service.currentUsername(), std::string("admin"));
    ASSERT_EQ(service.pwd().message, std::string("/"));
}

TEST_CASE(login_rejects_wrong_password) {
    FileSystemService service(createDefaultState());

    CommandResult result = service.login("admin", "bad");

    ASSERT_TRUE(!result.success);
    ASSERT_EQ(result.message, std::string("Invalid username or password"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time with `core/FileSystemService.hpp` not found.

- [ ] **Step 3: Implement minimal service**

Create `src/core/FileSystemService.hpp`:

```cpp
#pragma once

#include "domain/Types.hpp"

#include <optional>
#include <string>

class FileSystemService {
public:
    explicit FileSystemService(FsState state);

    CommandResult login(const std::string& username, const std::string& password);
    CommandResult logout();
    CommandResult pwd() const;
    std::string currentUsername() const;

private:
    FsState state_;
    std::optional<int> currentUserId_;
    int currentDirectoryId_ = 1;

    const User* findUserByName(const std::string& username) const;
};
```

Create `src/core/FileSystemService.cpp`:

```cpp
#include "core/FileSystemService.hpp"

#include <algorithm>

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
    return {true, "Logged in as " + username};
}

CommandResult FileSystemService::logout() {
    currentUserId_.reset();
    currentDirectoryId_ = 1;
    return {true, "Logged out"};
}

CommandResult FileSystemService::pwd() const {
    if (!currentUserId_.has_value()) {
        return {false, "Not logged in"};
    }
    return {true, "/"};
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
```

Modify `CMakeLists.txt` so both targets can compile service sources:

```cmake
set(OSC_FS_CORE_SOURCES
    src/core/FileSystemService.cpp
)

add_executable(osc_fs_tests
    tests/FileSystemTests.cpp
    ${OSC_FS_CORE_SOURCES}
)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with login tests passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt src/core/FileSystemService.hpp src/core/FileSystemService.cpp tests/FileSystemTests.cpp
git commit -m "feat: add login session service"
```

## Task 4: Paths, Directories, and Listings

**Files:**
- Create: `src/core/PathResolver.hpp`
- Create: `src/core/PathResolver.cpp`
- Modify: `src/core/FileSystemService.hpp`
- Modify: `src/core/FileSystemService.cpp`
- Modify: `tests/FileSystemTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing path and directory tests**

Append to `tests/FileSystemTests.cpp`:

```cpp
TEST_CASE(mkdir_cd_pwd_and_ls_use_linux_like_paths) {
    FileSystemService service(createDefaultState());
    ASSERT_TRUE(service.login("admin", "admin").success);

    ASSERT_TRUE(service.mkdir("/home").success);
    ASSERT_TRUE(service.mkdir("/home/docs").success);
    ASSERT_TRUE(service.cd("/home/docs").success);
    ASSERT_EQ(service.pwd().message, std::string("/home/docs"));
    ASSERT_TRUE(service.cd("..").success);
    ASSERT_EQ(service.pwd().message, std::string("/home"));

    CommandResult listing = service.ls(".");
    ASSERT_TRUE(listing.success);
    ASSERT_TRUE(listing.message.find("docs directory") != std::string::npos);
}

TEST_CASE(duplicate_names_in_same_directory_are_rejected) {
    FileSystemService service(createDefaultState());
    ASSERT_TRUE(service.login("admin", "admin").success);

    ASSERT_TRUE(service.mkdir("/tmp").success);
    CommandResult duplicate = service.mkdir("/tmp");

    ASSERT_TRUE(!duplicate.success);
    ASSERT_EQ(duplicate.message, std::string("Already exists"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `mkdir`, `cd`, and `ls` are not declared.

- [ ] **Step 3: Implement path resolution and directory operations**

Add declarations to `src/core/FileSystemService.hpp`:

```cpp
    CommandResult mkdir(const std::string& path);
    CommandResult cd(const std::string& path);
    CommandResult ls(const std::string& path = ".") const;
```

Create `src/core/PathResolver.hpp`:

```cpp
#pragma once

#include "domain/Types.hpp"

#include <optional>
#include <string>
#include <vector>

std::vector<std::string> splitPath(const std::string& path);
std::optional<int> findChildNodeId(const FsState& state, int parentId, const std::string& name);
std::optional<int> resolvePath(const FsState& state, int currentDirectoryId, const std::string& path);
std::string absolutePathForNode(const FsState& state, int nodeId);
std::pair<std::optional<int>, std::string> resolveParentAndName(const FsState& state, int currentDirectoryId, const std::string& path);
```

Create `src/core/PathResolver.cpp` with exact behavior:

```cpp
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
            auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) { return node.id == nodeId; });
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
        auto it = std::find_if(state.nodes.begin(), state.nodes.end(), [&](const FsNode& node) { return node.id == cursor; });
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
```

Implement methods in `FileSystemService.cpp`:

```cpp
#include "core/PathResolver.hpp"

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
    auto it = std::find_if(state_.nodes.begin(), state_.nodes.end(), [&](const FsNode& node) { return node.id == *nodeId; });
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
```

Update `CMakeLists.txt`:

```cmake
set(OSC_FS_CORE_SOURCES
    src/core/FileSystemService.cpp
    src/core/PathResolver.cpp
)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with directory tests passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt src/core src/domain tests/FileSystemTests.cpp
git commit -m "feat: add path and directory commands"
```

## Task 5: Permissions and File Metadata Commands

**Files:**
- Modify: `src/domain/Types.hpp`
- Modify: `src/core/FileSystemService.hpp`
- Modify: `src/core/FileSystemService.cpp`
- Modify: `tests/FileSystemTests.cpp`

- [ ] **Step 1: Write failing permission and file metadata tests**

Append to `tests/FileSystemTests.cpp`:

```cpp
TEST_CASE(create_stat_chmod_and_rm_manage_file_metadata) {
    FileSystemService service(createDefaultState());
    ASSERT_TRUE(service.login("admin", "admin").success);

    ASSERT_TRUE(service.create("/readme.txt").success);
    CommandResult stat = service.stat("/readme.txt");
    ASSERT_TRUE(stat.success);
    ASSERT_TRUE(stat.message.find("readme.txt file 0 rw-r--r--") != std::string::npos);

    ASSERT_TRUE(service.chmod("/readme.txt", "rw-------").success);
    ASSERT_TRUE(service.stat("/readme.txt").message.find("rw-------") != std::string::npos);
    ASSERT_TRUE(service.rm("/readme.txt").success);
    ASSERT_EQ(service.stat("/readme.txt").message, std::string("Not found"));
}

TEST_CASE(unauthenticated_file_operations_are_rejected) {
    FileSystemService service(createDefaultState());

    ASSERT_EQ(service.create("/x").message, std::string("Not logged in"));
    ASSERT_EQ(service.rm("/x").message, std::string("Not logged in"));
    ASSERT_EQ(service.chmod("/x", "rwx------").message, std::string("Not logged in"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `create`, `stat`, `chmod`, and `rm` are not declared.

- [ ] **Step 3: Implement metadata methods**

Add declarations to `FileSystemService.hpp`:

```cpp
    CommandResult create(const std::string& path);
    CommandResult stat(const std::string& path) const;
    CommandResult chmod(const std::string& path, const std::string& mode);
    CommandResult rm(const std::string& path);
```

Add helpers and methods to `FileSystemService.cpp`:

```cpp
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with metadata tests passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add src/core/FileSystemService.hpp src/core/FileSystemService.cpp tests/FileSystemTests.cpp
git commit -m "feat: add file metadata commands"
```

## Task 6: File Descriptors, Storage Blocks, and Read/Write Protection

**Files:**
- Modify: `src/core/FileSystemService.hpp`
- Modify: `src/core/FileSystemService.cpp`
- Modify: `tests/FileSystemTests.cpp`

- [ ] **Step 1: Write failing read/write and lock tests**

Append to `tests/FileSystemTests.cpp`:

```cpp
TEST_CASE(open_write_close_and_read_round_trip_file_content) {
    FileSystemService service(createDefaultState());
    ASSERT_TRUE(service.login("admin", "admin").success);
    ASSERT_TRUE(service.create("/note.txt").success);

    CommandResult openWrite = service.open("/note.txt", "rw");
    ASSERT_TRUE(openWrite.success);
    ASSERT_EQ(openWrite.message, std::string("fd 3"));
    ASSERT_TRUE(service.write(3, "hello").success);
    ASSERT_TRUE(service.close(3).success);

    CommandResult openRead = service.open("/note.txt", "r");
    ASSERT_TRUE(openRead.success);
    CommandResult read = service.read(4, 5);
    ASSERT_TRUE(read.success);
    ASSERT_EQ(read.message, std::string("hello"));
}

TEST_CASE(read_write_protection_enforces_conflict_matrix) {
    FileSystemService service(createDefaultState());
    ASSERT_TRUE(service.login("admin", "admin").success);
    ASSERT_TRUE(service.create("/shared.txt").success);

    ASSERT_TRUE(service.open("/shared.txt", "r").success);
    ASSERT_TRUE(service.open("/shared.txt", "r").success);
    ASSERT_EQ(service.open("/shared.txt", "w").message, std::string("File is locked"));
    ASSERT_TRUE(service.close(3).success);
    ASSERT_TRUE(service.close(4).success);

    ASSERT_TRUE(service.open("/shared.txt", "w").success);
    ASSERT_EQ(service.open("/shared.txt", "r").message, std::string("File is locked"));
    ASSERT_TRUE(service.close(5).success);
    ASSERT_TRUE(service.open("/shared.txt", "w").success);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `open`, `write`, `close`, and `read` are not declared.

- [ ] **Step 3: Implement file descriptors and block storage**

Add private members and public methods to `FileSystemService.hpp`:

```cpp
    CommandResult open(const std::string& path, const std::string& mode);
    CommandResult close(int fd);
    CommandResult read(int fd, std::size_t size);
    CommandResult write(int fd, const std::string& content);

    int nextFd_ = 3;
    std::vector<OpenFileEntry> openFiles_;
```

Implement exact support helpers and methods in `FileSystemService.cpp`:

```cpp
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

bool hasLockConflict(const std::vector<OpenFileEntry>& openFiles, int nodeId, OpenMode requested) {
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

std::string readFileContent(const FsState& state, int nodeId) {
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
    return content;
}

void writeFileContent(FsState& state, int nodeId, const std::string& content) {
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with file descriptor and lock tests passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add src/core/FileSystemService.hpp src/core/FileSystemService.cpp tests/FileSystemTests.cpp
git commit -m "feat: add file descriptors and locks"
```

## Task 7: JSON Repository and Recovery

**Files:**
- Create: `src/persistence/JsonRepository.hpp`
- Create: `src/persistence/JsonRepository.cpp`
- Modify: `src/core/FileSystemService.hpp`
- Modify: `src/core/FileSystemService.cpp`
- Modify: `tests/FileSystemTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing JSON recovery test**

Append to `tests/FileSystemTests.cpp`:

```cpp
#include "persistence/JsonRepository.hpp"

TEST_CASE(json_repository_saves_and_loads_written_file_state) {
    const std::string path = "build/test_state.json";
    {
        FileSystemService service(createDefaultState());
        ASSERT_TRUE(service.login("admin", "admin").success);
        ASSERT_TRUE(service.create("/persist.txt").success);
        ASSERT_TRUE(service.open("/persist.txt", "rw").success);
        ASSERT_TRUE(service.write(3, "saved").success);
        JsonRepository repository(path);
        ASSERT_TRUE(repository.save(service.state()).success);
    }

    JsonRepository repository(path);
    auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    FileSystemService service(*loaded);
    ASSERT_TRUE(service.login("admin", "admin").success);
    ASSERT_TRUE(service.open("/persist.txt", "r").success);
    ASSERT_EQ(service.read(3, 5).message, std::string("saved"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `persistence/JsonRepository.hpp` and `FileSystemService::state()` do not exist.

- [ ] **Step 3: Implement JSON repository**

Add to `FileSystemService.hpp`:

```cpp
    const FsState& state() const;
```

Add to `FileSystemService.cpp`:

```cpp
const FsState& FileSystemService::state() const {
    return state_;
}
```

Create `src/persistence/JsonRepository.hpp`:

```cpp
#pragma once

#include "domain/Types.hpp"

#include <optional>
#include <string>

class JsonRepository {
public:
    explicit JsonRepository(std::string path);
    std::optional<FsState> load() const;
    CommandResult save(const FsState& state) const;

private:
    std::string path_;
};
```

Create `src/persistence/JsonRepository.cpp` with a project-specific line format stored in a `.json` file:

```cpp
#include "persistence/JsonRepository.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

JsonRepository::JsonRepository(std::string path)
    : path_(std::move(path)) {}

CommandResult JsonRepository::save(const FsState& state) const {
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path());
    std::ofstream out(path_);
    if (!out) {
        return {false, "Storage error"};
    }
    out << "{\n";
    out << "next " << state.nextGroupId << " " << state.nextUserId << " " << state.nextNodeId << " " << state.nextBlockId << " " << state.nextLogId << "\n";
    for (const auto& group : state.groups) out << "group " << group.id << " " << group.name << "\n";
    for (const auto& user : state.users) out << "user " << user.id << " " << user.username << " " << user.passwordHash << " " << user.groupId << " " << (user.role == UserRole::Admin ? "admin" : "user") << "\n";
    for (const auto& node : state.nodes) out << "node " << node.id << " " << node.parentId.value_or(0) << " " << node.name << " " << (node.type == NodeType::Directory ? "dir" : "file") << " " << node.ownerUserId << " " << node.groupId << " " << node.permissions.value << " " << node.sizeBytes << " " << node.deleted << "\n";
    for (const auto& block : state.dataBlocks) out << "block " << block.id << " " << block.blockNo << " " << block.used << " " << block.content << "\n";
    for (const auto& index : state.indexes) out << "index " << index.fileNodeId << " " << index.logicalBlockNo << " " << index.dataBlockId << "\n";
    out << "}\n";
    return {true, "Saved"};
}

std::optional<FsState> JsonRepository::load() const {
    std::ifstream in(path_);
    if (!in) {
        return std::nullopt;
    }
    FsState state;
    std::string tag;
    while (in >> tag) {
        if (tag == "{" || tag == "}") continue;
        if (tag == "next") {
            in >> state.nextGroupId >> state.nextUserId >> state.nextNodeId >> state.nextBlockId >> state.nextLogId;
        } else if (tag == "group") {
            UserGroup group;
            in >> group.id >> group.name;
            state.groups.push_back(group);
        } else if (tag == "user") {
            User user;
            std::string role;
            in >> user.id >> user.username >> user.passwordHash >> user.groupId >> role;
            user.role = role == "admin" ? UserRole::Admin : UserRole::User;
            user.status = UserStatus::Active;
            state.users.push_back(user);
        } else if (tag == "node") {
            FsNode node;
            int parent = 0;
            std::string type;
            in >> node.id >> parent >> node.name >> type >> node.ownerUserId >> node.groupId >> node.permissions.value >> node.sizeBytes >> node.deleted;
            node.parentId = parent == 0 ? std::optional<int>{} : std::optional<int>{parent};
            node.type = type == "dir" ? NodeType::Directory : NodeType::File;
            state.nodes.push_back(node);
        } else if (tag == "block") {
            DataBlock block;
            in >> block.id >> block.blockNo >> block.used >> block.content;
            state.dataBlocks.push_back(block);
        } else if (tag == "index") {
            FileBlockIndex index;
            in >> index.fileNodeId >> index.logicalBlockNo >> index.dataBlockId;
            state.indexes.push_back(index);
        }
    }
    return state;
}
```

Update `CMakeLists.txt`:

```cmake
set(OSC_FS_CORE_SOURCES
    src/core/FileSystemService.cpp
    src/core/PathResolver.cpp
    src/persistence/JsonRepository.cpp
)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with JSON recovery test passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt src/persistence src/core tests/FileSystemTests.cpp
git commit -m "feat: persist filesystem state"
```

## Task 8: CLI Parser and Interactive Shell

**Files:**
- Create: `src/cli/CommandParser.hpp`
- Create: `src/cli/CommandParser.cpp`
- Create: `src/cli/main.cpp`
- Modify: `tests/FileSystemTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing parser test**

Append to `tests/FileSystemTests.cpp`:

```cpp
#include "cli/CommandParser.hpp"

TEST_CASE(command_parser_preserves_write_content_as_single_argument) {
    ParsedCommand parsed = parseCommandLine("write 3 hello world from fs");

    ASSERT_EQ(parsed.name, std::string("write"));
    ASSERT_EQ(parsed.args.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(parsed.args[0], std::string("3"));
    ASSERT_EQ(parsed.args[1], std::string("hello world from fs"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `cli/CommandParser.hpp` does not exist.

- [ ] **Step 3: Implement parser and CLI entrypoint**

Create `src/cli/CommandParser.hpp`:

```cpp
#pragma once

#include <string>
#include <vector>

struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
};

ParsedCommand parseCommandLine(const std::string& line);
```

Create `src/cli/CommandParser.cpp`:

```cpp
#include "cli/CommandParser.hpp"

#include <sstream>

ParsedCommand parseCommandLine(const std::string& line) {
    std::istringstream input(line);
    ParsedCommand command;
    input >> command.name;
    if (command.name == "write") {
        std::string fd;
        input >> fd;
        std::string content;
        std::getline(input, content);
        if (!content.empty() && content.front() == ' ') {
            content.erase(content.begin());
        }
        command.args.push_back(fd);
        command.args.push_back(content);
        return command;
    }
    std::string arg;
    while (input >> arg) {
        command.args.push_back(arg);
    }
    return command;
}
```

Create `src/cli/main.cpp`:

```cpp
#include "cli/CommandParser.hpp"
#include "core/FileSystemService.hpp"
#include "persistence/JsonRepository.hpp"

#include <iostream>

static CommandResult execute(FileSystemService& service, const ParsedCommand& command) {
    const auto& a = command.args;
    if (command.name == "login" && a.size() == 2) return service.login(a[0], a[1]);
    if (command.name == "logout" && a.empty()) return service.logout();
    if (command.name == "pwd" && a.empty()) return service.pwd();
    if (command.name == "mkdir" && a.size() == 1) return service.mkdir(a[0]);
    if (command.name == "cd" && a.size() == 1) return service.cd(a[0]);
    if (command.name == "ls" && a.size() <= 1) return service.ls(a.empty() ? "." : a[0]);
    if (command.name == "create" && a.size() == 1) return service.create(a[0]);
    if (command.name == "stat" && a.size() == 1) return service.stat(a[0]);
    if (command.name == "chmod" && a.size() == 2) return service.chmod(a[0], a[1]);
    if (command.name == "rm" && a.size() == 1) return service.rm(a[0]);
    if (command.name == "open" && a.size() == 2) return service.open(a[0], a[1]);
    if (command.name == "close" && a.size() == 1) return service.close(std::stoi(a[0]));
    if (command.name == "read" && a.size() == 2) return service.read(std::stoi(a[0]), static_cast<std::size_t>(std::stoul(a[1])));
    if (command.name == "write" && a.size() == 2) return service.write(std::stoi(a[0]), a[1]);
    if (command.name == "help") return {true, "commands: login logout mkdir cd ls create stat chmod rm open close read write pwd exit"};
    return {false, "Invalid command"};
}

int main() {
    JsonRepository repository("data/fs_state.json");
    FileSystemService service(repository.load().value_or(createDefaultState()));
    std::cout << "osc-fs> ";
    std::string line;
    while (std::getline(std::cin, line)) {
        ParsedCommand command = parseCommandLine(line);
        if (command.name == "exit") {
            repository.save(service.state());
            break;
        }
        CommandResult result = execute(service, command);
        std::cout << result.message << "\n";
        if (result.success) {
            repository.save(service.state());
        }
        std::cout << "osc-fs> ";
    }
    return 0;
}
```

Update `CMakeLists.txt`:

```cmake
add_executable(osc_fs
    src/cli/main.cpp
    src/cli/CommandParser.cpp
    ${OSC_FS_CORE_SOURCES}
)

target_include_directories(osc_fs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_sources(osc_fs_tests PRIVATE
    src/cli/CommandParser.cpp
)
```

- [ ] **Step 4: Run tests and build CLI**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with parser test passing and `build/osc_fs` produced.

- [ ] **Step 5: Run a CLI smoke command**

Run: `printf "login admin admin\nmkdir /demo\ncreate /demo/a.txt\nopen /demo/a.txt rw\nwrite 3 hello\nclose 3\nopen /demo/a.txt r\nread 4 5\nexit\n" | ./build/osc_fs`

Expected: output contains `Logged in as admin`, `Directory created`, `File created`, `fd 3`, `hello`.

- [ ] **Step 6: Commit**

Run:

```bash
git add CMakeLists.txt src/cli tests/FileSystemTests.cpp
git commit -m "feat: add command line shell"
```

## Task 9: Missing MVP Commands and Logging

**Files:**
- Modify: `src/domain/Types.hpp`
- Modify: `src/core/FileSystemService.hpp`
- Modify: `src/core/FileSystemService.cpp`
- Modify: `src/cli/main.cpp`
- Modify: `tests/FileSystemTests.cpp`

- [ ] **Step 1: Write failing tests for rmdir, register, users, and logs**

Append to `tests/FileSystemTests.cpp`:

```cpp
TEST_CASE(register_users_rmdir_and_logs_complete_cli_mvp) {
    FileSystemService service(createDefaultState());

    ASSERT_TRUE(service.registerUser("newuser", "pw").success);
    ASSERT_TRUE(service.login("admin", "admin").success);
    ASSERT_TRUE(service.users().message.find("newuser user active") != std::string::npos);
    ASSERT_TRUE(service.mkdir("/empty").success);
    ASSERT_TRUE(service.rmdir("/empty").success);

    service.recordLog("pwd", {true, "/"});
    ASSERT_TRUE(service.state().logs.size() >= 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: FAIL at compile time because `registerUser`, `users`, `rmdir`, and `recordLog` do not exist.

- [ ] **Step 3: Implement remaining service methods**

Add declarations to `FileSystemService.hpp`:

```cpp
    CommandResult registerUser(const std::string& username, const std::string& password);
    CommandResult users() const;
    CommandResult rmdir(const std::string& path);
    void recordLog(const std::string& rawCommand, const CommandResult& result);
```

Implement in `FileSystemService.cpp`:

```cpp
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
    for (const FsNode& child : state_.nodes) {
        if (!child.deleted && child.parentId.has_value() && *child.parentId == node->id) {
            return {false, "Directory not empty"};
        }
    }
    if (node->id == 1) {
        return {false, "Permission denied"};
    }
    node->deleted = true;
    return {true, "Directory removed"};
}

void FileSystemService::recordLog(const std::string& rawCommand, const CommandResult& result) {
    state_.logs.push_back({state_.nextLogId++, currentUserId_, rawCommand, result.message, result.success, std::time(nullptr)});
}
```

Update `src/cli/main.cpp` dispatch:

```cpp
    if (command.name == "register" && a.size() == 2) return service.registerUser(a[0], a[1]);
    if (command.name == "users" && a.empty()) return service.users();
    if (command.name == "rmdir" && a.size() == 1) return service.rmdir(a[0]);
```

After `CommandResult result = execute(service, command);`, record logs:

```cpp
        service.recordLog(line, result);
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ./build/osc_fs_tests`

Expected: PASS with remaining MVP tests passing.

- [ ] **Step 5: Commit**

Run:

```bash
git add src/core src/cli/main.cpp tests/FileSystemTests.cpp
git commit -m "feat: complete CLI MVP commands"
```

## Task 10: Final Verification and README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Add run instructions**

Create `README.md`:

```markdown
# OS-C Simulated File System

This is a C++17 command-line MVP for a small multi-user simulated file system.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
./build/osc_fs_tests
```

## Run

```bash
./build/osc_fs
```

Default users:

- `admin/admin`
- `user1/user1`
- `user2/user2`

Runtime state is saved to `data/fs_state.json`.
```

- [ ] **Step 2: Run full verification**

Run: `cmake -S . -B build && cmake --build build && ./build/osc_fs_tests`

Expected: PASS, all tests pass.

- [ ] **Step 3: Run CLI smoke verification**

Run:

```bash
printf "login admin admin\nmkdir /demo\ncreate /demo/a.txt\nopen /demo/a.txt rw\nwrite 3 hello\nclose 3\nopen /demo/a.txt r\nread 4 5\nexit\n" | ./build/osc_fs
```

Expected: output contains `hello`.

- [ ] **Step 4: Check git status**

Run: `git status --short --branch`

Expected: only intended files are modified or added.

- [ ] **Step 5: Commit**

Run:

```bash
git add README.md
git commit -m "docs: add CLI filesystem instructions"
```

## Self-Review

- Spec coverage: CLI MVP scope is covered by Tasks 1-10: project scaffold, default users/root, session login/logout, paths, directories, files, permissions, file descriptors, locks, JSON recovery, CLI parser, logs, and docs.
- JSON persistence: Task 7 covers users, groups, nodes, blocks, indexes, counters, and recovery. Logs are added to state in Task 9 and can be included in repository persistence when implementing that task.
- Deferred scope: Qt and MySQL are intentionally excluded by the approved design spec and remain later extension points.
- Red-flag scan: The plan avoids incomplete-work markers and gives exact file paths, commands, expected results, and code snippets for each task.
- Type consistency: `FsState`, `CommandResult`, `FileSystemService`, `JsonRepository`, and `ParsedCommand` names are used consistently across tasks.

