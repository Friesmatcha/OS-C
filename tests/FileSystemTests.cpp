#include "TestHarness.hpp"

#include "domain/Types.hpp"
#include "core/FileSystemService.hpp"
#include "persistence/JsonRepository.hpp"
#include "cli/CommandParser.hpp"

TEST_CASE(smoke_test_runner_executes_tests) {
    ASSERT_TRUE(true);
    ASSERT_EQ(1 + 1, 2);
}

TEST_CASE(default_state_contains_three_users_and_root) {
    FsState state = createDefaultState();

    ASSERT_EQ(state.users.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(state.groups.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(state.nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(state.nodes.front().name, std::string("/"));
    ASSERT_EQ(state.nodes.front().permissions.value, std::string("rwxr-xr-x"));
}

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

TEST_CASE(command_parser_preserves_write_content_as_single_argument) {
    ParsedCommand parsed = parseCommandLine("write 3 hello world from fs");

    ASSERT_EQ(parsed.name, std::string("write"));
    ASSERT_EQ(parsed.args.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(parsed.args[0], std::string("3"));
    ASSERT_EQ(parsed.args[1], std::string("hello world from fs"));
}

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

TEST_CASE(permission_bits_reject_unauthorized_users_and_allow_admin_override) {
    FileSystemService service(createDefaultState());

    ASSERT_TRUE(service.login("user1", "user1").success);
    ASSERT_EQ(service.create("/blocked.txt").message, std::string("Permission denied"));
    ASSERT_TRUE(service.logout().success);

    ASSERT_TRUE(service.login("admin", "admin").success);
    ASSERT_TRUE(service.mkdir("/shared").success);
    ASSERT_TRUE(service.chmod("/shared", "rwxrwxrwx").success);
    ASSERT_TRUE(service.create("/secret.txt").success);
    ASSERT_TRUE(service.chmod("/secret.txt", "rw-------").success);
    ASSERT_TRUE(service.logout().success);

    ASSERT_TRUE(service.login("user1", "user1").success);
    ASSERT_TRUE(service.create("/shared/mine.txt").success);
    ASSERT_EQ(service.open("/secret.txt", "r").message, std::string("Permission denied"));
    ASSERT_EQ(service.stat("/secret.txt").message, std::string("Permission denied"));
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

