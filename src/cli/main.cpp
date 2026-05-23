#include "cli/CommandParser.hpp"
#include "core/FileSystemService.hpp"
#include "persistence/JsonRepository.hpp"

#include <iostream>
#include <stdexcept>

static CommandResult execute(FileSystemService& service, const ParsedCommand& command) {
    const auto& a = command.args;
    try {
        if (command.name == "register" && a.size() == 2) return service.registerUser(a[0], a[1]);
        if (command.name == "login" && a.size() == 2) return service.login(a[0], a[1]);
        if (command.name == "logout" && a.empty()) return service.logout();
        if (command.name == "pwd" && a.empty()) return service.pwd();
        if (command.name == "mkdir" && a.size() == 1) return service.mkdir(a[0]);
        if (command.name == "rmdir" && a.size() == 1) return service.rmdir(a[0]);
        if (command.name == "users" && a.empty()) return service.users();
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
        if (command.name == "help") return {true, "commands: register login logout users mkdir rmdir cd ls create stat chmod rm open close read write pwd exit"};
    } catch (const std::exception&) {
        return {false, "Invalid arguments"};
    }
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
        service.recordLog(line, result);
        std::cout << result.message << "\n";
        if (result.success) {
            repository.save(service.state());
        }
        std::cout << "osc-fs> ";
    }
    return 0;
}

