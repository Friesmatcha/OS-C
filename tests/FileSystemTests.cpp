#include "TestHarness.hpp"

#include "domain/Types.hpp"

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

