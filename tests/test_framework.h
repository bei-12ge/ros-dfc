#pragma once
// ---------------------------------------------------------------------------
// 极简测试框架（不依赖 gtest，便于零依赖构建）
//   * TEST_CASE(套件, 用例名) 注册用例，main 中统一执行；
//   * CHECK / CHECK_EQ / CHECK_STR_EQ / CHECK_THROWS_CODE / CHECK_NO_THROW 断言；
//   * 统计断言次数与失败次数，任一失败则进程退出码为 1（可用于 CI）。
// ---------------------------------------------------------------------------
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "ros/exception.h"

namespace testing {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Stats {
    int checks = 0;
    int failures = 0;
};

inline Stats& stats() {
    static Stats s;
    return s;
}

struct Registrar {
    Registrar(const std::string& suite, const std::string& name, std::function<void()> body) {
        registry().push_back(TestCase{suite, name, std::move(body)});
    }
};

inline void reportFailure(const std::string& file, int line, const std::string& detail) {
    ++stats().failures;
    std::cout << "    \033[31m[失败]\033[0m " << file << ":" << line << " → " << detail << "\n";
}

}  // namespace testing

// ------------------------------------------------------------------ 用例注册
#define TEST_CASE(suite, name)                                                       \
    static void test_##suite##_##name();                                             \
    static ::testing::Registrar registrar_##suite##_##name(#suite, #name,            \
                                                           test_##suite##_##name);   \
    static void test_##suite##_##name()

// ------------------------------------------------------------------ 断言
#define CHECK(condition)                                                             \
    do {                                                                             \
        ++::testing::stats().checks;                                                 \
        if (!(condition)) {                                                          \
            ::testing::reportFailure(__FILE__, __LINE__, "条件不成立: " #condition);  \
        }                                                                            \
    } while (0)

#define CHECK_EQ(actual, expected)                                                   \
    do {                                                                             \
        ++::testing::stats().checks;                                                 \
        const long long actualValue = static_cast<long long>(actual);                \
        const long long expectedValue = static_cast<long long>(expected);            \
        if (actualValue != expectedValue) {                                          \
            ::testing::reportFailure(__FILE__, __LINE__,                              \
                                     std::string(#actual) + " = " +                  \
                                         std::to_string(actualValue) +               \
                                         "，期望 " + std::to_string(expectedValue));  \
        }                                                                            \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                               \
    do {                                                                             \
        ++::testing::stats().checks;                                                 \
        const std::string actualValue = (actual);                                    \
        if (actualValue != (expected)) {                                             \
            ::testing::reportFailure(__FILE__, __LINE__,                              \
                                     std::string(#actual) + " = \"" + actualValue +  \
                                         "\"，期望 \"" + std::string(expected) + "\""); \
        }                                                                            \
    } while (0)

#define CHECK_THROWS_CODE(expectedCode, ...)                                         \
    do {                                                                             \
        ++::testing::stats().checks;                                                 \
        bool thrown = false;                                                         \
        std::string code = "<未抛出>";                                                \
        try {                                                                        \
            __VA_ARGS__;                                                             \
        } catch (const ::ros_sim::RosException& ex) {                                \
            thrown = true;                                                           \
            code = ex.code();                                                        \
        } catch (const std::exception&) {                                            \
            thrown = true;                                                           \
            code = "<std::exception 而非 RosException>";                             \
        }                                                                            \
        if (!thrown || code != (expectedCode)) {                                     \
            ::testing::reportFailure(__FILE__, __LINE__,                              \
                                     "期望异常码 " + std::string(expectedCode) +      \
                                         "，实际 " + code);                           \
        }                                                                            \
    } while (0)

#define CHECK_NO_THROW(...)                                                          \
    do {                                                                             \
        ++::testing::stats().checks;                                                 \
        try {                                                                        \
            __VA_ARGS__;                                                             \
        } catch (const std::exception& ex) {                                         \
            ::testing::reportFailure(__FILE__, __LINE__,                              \
                                     std::string("未预期的异常: ") + ex.what());       \
        }                                                                            \
    } while (0)
