#include "core/FileSystemService.hpp"

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

