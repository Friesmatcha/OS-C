#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& testCases() {
    static std::vector<TestCase> cases;
    return cases;
}

struct TestRegistrar {
    TestRegistrar(std::string name, std::function<void()> body) {
        testCases().push_back({std::move(name), std::move(body)});
    }
};

#define TEST_CASE(name) \
    static void name(); \
    static TestRegistrar registrar_##name(#name, name); \
    static void name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(std::string("ASSERT_TRUE failed: ") + #expr); \
        } \
    } while (false)

#define ASSERT_EQ(actual, expected) \
    do { \
        auto actualValue = (actual); \
        auto expectedValue = (expected); \
        if (!(actualValue == expectedValue)) { \
            std::ostringstream assertionStream; \
            assertionStream << "ASSERT_EQ failed: " << #actual << " != " << #expected \
                            << " (actual: " << actualValue << ", expected: " << expectedValue << ")"; \
            throw std::runtime_error(assertionStream.str()); \
        } \
    } while (false)

