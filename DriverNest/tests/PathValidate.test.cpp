#include "test_framework.h"
#include <string>
#include <cctype>

namespace {

// Path validation helper (mirrors the one used in Installer).
bool IsPathValid(std::wstring_view path) {
    if (path.empty()) return false;
    for (wchar_t c : path) {
        if (c == L'"' || (c >= 0 && c < 32)) {
            return false;
        }
    }
    return true;
}

void TestRejectsQuotes() {
    TEST_ASSERT(!IsPathValid(L"test\"quote.inf"));
    TEST_ASSERT(IsPathValid(L"test'in.inf"));  // single quotes are legal in file names
    TEST_ASSERT(!IsPathValid(L"test\"quote\"path.inf"));
}

void TestRejectsControlChars() {
    // Null character
    std::wstring bad1 = L"test";
    bad1 += L'\0';
    bad1 += L".inf";
    TEST_ASSERT(!IsPathValid(bad1));

    // Other control characters
    std::wstring bad2 = L"test";
    bad2 += L'\r';
    bad2 += L".inf";
    TEST_ASSERT(!IsPathValid(bad2));

    std::wstring bad3 = L"test";
    bad3 += L'\n';
    bad3 += L".inf";
    TEST_ASSERT(!IsPathValid(bad3));
}

void TestAcceptsNormalPaths() {
    TEST_ASSERT(IsPathValid(L"test.inf"));
    TEST_ASSERT(IsPathValid(L"C:\\Windows\\System32\\drivers\\test.inf"));
    TEST_ASSERT(IsPathValid(L"relative\\path\\test.inf"));
    TEST_ASSERT(IsPathValid(L"test with spaces.inf"));
}

void TestAcceptsUnicode() {
    // Unicode paths are valid
    TEST_ASSERT(IsPathValid(L"テスト.inf"));
    TEST_ASSERT(IsPathValid(L" prueba .inf"));
}

} // namespace

TEST(PathValidateRejectsQuotes) { TestRejectsQuotes(); }
TEST(PathValidateRejectsControlChars) { TestRejectsControlChars(); }
TEST(PathValidateAcceptsNormalPaths) { TestAcceptsNormalPaths(); }
TEST(PathValidateAcceptsUnicode) { TestAcceptsUnicode(); }
