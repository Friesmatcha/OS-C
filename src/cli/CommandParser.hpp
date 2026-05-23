#pragma once

#include <string>
#include <vector>

struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
};

ParsedCommand parseCommandLine(const std::string& line);

