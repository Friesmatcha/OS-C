#pragma once

#include "domain/Types.hpp"

#include <optional>
#include <string>
#include <vector>

class FileSystemService {
public:
    explicit FileSystemService(FsState state);

    CommandResult login(const std::string& username, const std::string& password);
    CommandResult logout();
    CommandResult pwd() const;
    CommandResult mkdir(const std::string& path);
    CommandResult cd(const std::string& path);
    CommandResult ls(const std::string& path = ".") const;
    CommandResult create(const std::string& path);
    CommandResult stat(const std::string& path) const;
    CommandResult chmod(const std::string& path, const std::string& mode);
    CommandResult rm(const std::string& path);
    CommandResult open(const std::string& path, const std::string& mode);
    CommandResult close(int fd);
    CommandResult read(int fd, std::size_t size);
    CommandResult write(int fd, const std::string& content);
    std::string currentUsername() const;
    const FsState& state() const;

private:
    FsState state_;
    std::optional<int> currentUserId_;
    int currentDirectoryId_ = 1;
    int nextFd_ = 3;
    std::vector<OpenFileEntry> openFiles_;

    const User* findUserByName(const std::string& username) const;
};

