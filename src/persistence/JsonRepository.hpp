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

