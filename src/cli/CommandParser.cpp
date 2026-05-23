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
        if (!fd.empty()) {
            command.args.push_back(fd);
        }
        command.args.push_back(content);
        return command;
    }

    std::string arg;
    while (input >> arg) {
        command.args.push_back(arg);
    }
    return command;
}

