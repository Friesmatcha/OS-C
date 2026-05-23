#include "TestHarness.hpp"

#include "domain/Types.hpp"
#include "core/FileSystemService.hpp"

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

