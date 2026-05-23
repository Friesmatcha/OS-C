# CLI JSON File System MVP Design

## Context

The repository currently contains the PRD for a small multi-user simulated file system. There is no existing C++ or Qt codebase. The first implementation target is a command-line MVP that proves the core file system behavior before adding Qt or MySQL.

The MVP uses C++17 and JSON file persistence. MySQL remains a later storage backend because the local machine does not currently have MySQL or MariaDB installed.

## Goals

- Implement the file system core in a way that can later be reused by Qt.
- Support a Linux-like command-line workflow for users, directories, files, permissions, file descriptors, and read/write protection.
- Persist users, nodes, file blocks, block indexes, and operation logs to a local JSON file.
- Keep storage behind a repository boundary so JSON can later be replaced by MySQL.
- Provide focused tests for path resolution, permissions, file operations, lock behavior, and JSON recovery.

## Non-Goals

- No Qt UI in this phase.
- No direct MySQL integration in this phase.
- No real Linux filesystem mounting, kernel changes, POSIX compatibility, or binary large-file support.
- No multi-process concurrency. Read/write conflict simulation is managed inside one running CLI process.

## Approach

Use a layered C++17 project:

- `src/domain/`: core data structures such as users, groups, filesystem nodes, permissions, data blocks, open file entries, and operation logs.
- `src/core/`: business logic services such as `FileSystemService`, `PathResolver`, `PermissionManager`, `StorageManager`, and `OpenFileManager`.
- `src/persistence/`: JSON repository implementation for loading and saving file system state.
- `src/cli/`: command parser and interactive command-line shell.
- `tests/`: focused automated tests for the required behavior.

The CLI talks only to `FileSystemService`. It does not directly mutate JSON state or domain objects. This keeps the core reusable when Qt is added later.

## Initial State

When `data/fs_state.json` does not exist, the application initializes:

- `admin/admin`, role `admin`, group `admin`
- `user1/user1`, role `user`, group `users`
- `user2/user2`, role `user`, group `users`
- root directory `/`, owned by `admin`, group `admin`, permissions `rwxr-xr-x`

Passwords are stored as hashes in the persisted state. For this MVP, password hashing is isolated behind a helper. If no crypto dependency is added, the implementation uses a clearly named development-only hash function so a stronger hash can replace it later without changing user-management logic.

## Command Scope

The MVP supports:

- `register username password`
- `login username password`
- `logout`
- `users`
- `mkdir path`
- `rmdir path`
- `create path`
- `open path r|w|rw`
- `close fd`
- `read fd size`
- `write fd content`
- `ls [path]`
- `cd path`
- `pwd`
- `chmod path mode`
- `stat path`
- `rm path`
- `help`
- `exit`

Each command returns a clear text result. Every command execution is recorded in the operation log with the raw command text, current user if any, result, success flag, and timestamp.

## Session Model

One CLI process has one current session:

- Current user is empty until `login` succeeds.
- Current directory starts at `/` after login.
- `logout` clears current user, current directory, and active file descriptors for that session.
- File operations that require authentication return `Not logged in` when no user is active.

## Paths

`PathResolver` supports:

- Absolute paths such as `/home/user/a.txt`
- Relative paths such as `docs/readme.txt`
- `.` and `..`
- Repeated slash normalization

Path traversal requires execute permission on each traversed directory unless the current user is admin. Directory listing requires read permission on the listed directory.

## Permissions

Permissions use a nine-character Linux-like string, for example `rwxr-x---`.

Checks:

- `read`, `ls`, and `stat` require read permission on the target.
- `write` requires write permission on the opened file.
- `create`, `mkdir`, `rm`, and `rmdir` require write and execute permission on the parent directory.
- `cd` and path traversal require execute permission on each directory.
- `chmod` is allowed for the node owner and for admin.
- Admin bypasses regular permission checks.

Denied operations return `Permission denied`.

## File Storage

File content is stored through fixed-size simulated data blocks:

- Block size: 512 bytes.
- A file node stores its logical size.
- `file_block_index` maps a file node to ordered data block IDs.
- Writes allocate new blocks as needed.
- Removing a file releases its blocks and removes its block index entries.

For the CLI MVP, text content is sufficient. The JSON format persists block content as strings.

## File Descriptors and Protection

`open path mode` returns an integer file descriptor scoped to the running process.

Open modes:

- `r`: read only
- `w`: write only
- `rw`: read and write

Offsets:

- Each `OpenFileEntry` has its own byte offset.
- `read fd size` reads from the current offset and advances it by the number of bytes returned.
- `write fd content` writes from the current offset and advances it by the number of bytes written.
- `open w` does not truncate the file automatically.

Conflict rules:

| Current active state | New `r` | New `w` | New `rw` |
|---|---:|---:|---:|
| Not open | allow | allow | allow |
| One or more `r` | allow | deny | deny |
| Any `w` | deny | deny | deny |
| Any `rw` | deny | deny | deny |

Closing a file descriptor releases its protection state. On application startup, active open state is empty even though persisted file content and metadata are restored.

## Persistence

The JSON repository saves and loads:

- users
- groups
- filesystem nodes
- data blocks
- file block indexes
- operation logs
- next ID counters

Default path: `data/fs_state.json`.

The repository writes the full state after successful mutating commands. If writing fails, the command returns a storage error and the service must avoid reporting success.

## Error Handling

The service returns structured command results with:

- success flag
- user-facing message
- optional payload for listings and stats

Common messages:

- `Not logged in`
- `Invalid command`
- `Invalid arguments`
- `Not found`
- `Already exists`
- `Not a directory`
- `Not a file`
- `Directory not empty`
- `Permission denied`
- `Invalid fd`
- `File is locked`
- `Storage error`

## Testing

Use focused automated tests before implementation code.

Required test groups:

- Path resolution: root, absolute paths, relative paths, `.`, `..`, repeated slashes, missing nodes.
- Permissions: unauthenticated rejection, owner/group/other checks, directory execute traversal, admin override.
- File operations: create, mkdir, rmdir, rm, open, close, read, write, stat, ls, pwd, cd.
- Protection: read/read allowed, read/write denied, write/read denied, write/write denied, close releases locks.
- JSON recovery: create directories and files, write content, chmod, save, reload, and verify restored state.

## Later Extension Points

- Replace JSON repository with a MySQL repository using the same core service interfaces.
- Add Qt UI that calls `FileSystemService` instead of touching persistence directly.
- Add data block visualization, open file table visualization, and operation log tables.
- Add indirect indexes, rename, import/export, and multi-session simulation.

