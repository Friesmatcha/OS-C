#include "TestHarness.hpp"

TEST_CASE(smoke_test_runner_executes_tests) {
    ASSERT_TRUE(true);
    ASSERT_EQ(1 + 1, 2);
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

