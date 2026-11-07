#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
// Minimal test runner.
//
// Assertions throw TestFailure, which the runner catches. A failed assertion
// therefore aborts only the current test case and is counted as a failure.
// ---------------------------------------------------------------------------

struct TestCase {
    const char* name;
    void (*func)();
};

struct TestFailure {
    std::string message;
};

inline std::vector<TestCase>& GetTests() {
    static std::vector<TestCase> tests;
    return tests;
}

// Narrow a wide string for assertion diagnostics.
inline std::string Narrow(const wchar_t* s) {
    if (!s) return "(null)";
    std::string out;
    for (; *s; ++s) out.push_back(static_cast<char>(*s));
    return out;
}

// Render a value for assertion diagnostics (std::string, wide string, or any
// streamable type).
template <typename T>
std::string Describe(const T& value) {
    if constexpr (std::is_same_v<T, std::wstring>) {
        std::string narrow(value.begin(), value.end());
        return narrow;
    } else if constexpr (std::is_same_v<T, std::wstring_view>) {
        return std::string(value.begin(), value.end());
    } else if constexpr (std::is_convertible_v<T, std::string>) {
        return std::string(value);
    } else {
        std::ostringstream os;
        os << value;
        return os.str();
    }
}

#define TEST(name) \
    static void Test_##name(); \
    static struct TestReg_##name { \
        TestReg_##name() { \
            GetTests().push_back({ #name, Test_##name }); \
        } \
    } _testReg_##name; \
    static void Test_##name()

#define FAIL_WITH(msg) \
    throw TestFailure{ std::string(msg) + " at line " + std::to_string(__LINE__) }

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            FAIL_WITH(std::string("assertion failed: ") + #cond); \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            FAIL_WITH(std::string("expected ") + #a + " == " + #b + \
                      " (got \"" + Describe(a) + "\" vs \"" + Describe(b) + "\")"); \
        } \
    } while (0)

#define TEST_ASSERT_STREQ(a, b) \
    do { \
        if (std::strcmp((a), (b)) != 0) { \
            FAIL_WITH(std::string("expected ") + #a + " == " + #b + \
                      " (\"" + (a) + "\" vs \"" + (b) + "\")"); \
        } \
    } while (0)

#define TEST_ASSERT_WSTREQ(a, b) \
    do { \
        if (std::wcscmp((a), (b)) != 0) { \
            FAIL_WITH(std::string("expected ") + #a + " == " + #b + \
                      " (got \"" + Narrow((a)) + "\" vs \"" + Narrow((b)) + "\")"); \
        } \
    } while (0)

// Marks the remainder of the test as intentionally not applicable.
#define SKIP(msg) \
    do { \
        std::cout << "[skip] " << msg << " "; \
        return; \
    } while (0)
