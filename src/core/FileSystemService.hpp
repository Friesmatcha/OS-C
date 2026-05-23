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
    std::string currentUsername() const;

private:
    FsState state_;
    std::optional<int> currentUserId_;
    int currentDirectoryId_ = 1;
    int nextFd_ = 3;
    std::vector<OpenFileEntry> openFiles_;

    const User* findUserByName(const std::string& username) const;
};

