# OS-C Simulated File System

This is a C++17 command-line MVP for a small multi-user simulated file system.

## Build

```bash
make all
```

## Test

```bash
make test
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

Supported commands:

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

