#include "test_framework.h"

int main() {
    auto& tests = GetTests();
    int passed = 0;
    int failed = 0;

    std::cout << "DriverNest Tests - " << tests.size() << " test cases" << std::endl;
    std::cout << "==========================================" << std::endl;

    for (auto& test : tests) {
        std::cout << "Running: " << test.name << " ... " << std::flush;
        try {
            test.func();
            std::cout << "PASSED" << std::endl;
            ++passed;
        } catch (const TestFailure& failure) {
            std::cout << "FAILED" << std::endl;
            std::cout << "  " << failure.message << std::endl;
            ++failed;
        }
    }

    std::cout << "==========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;

    return failed > 0 ? 1 : 0;
}
