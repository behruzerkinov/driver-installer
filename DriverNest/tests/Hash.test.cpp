#include "test_framework.h"
#include "Hash.h"
#include <string>

namespace {

void TestSha256KnownVector() {
    std::string result = DriverNest::Net::Sha256KnownVector();
    std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    TEST_ASSERT_EQ(result, expected);
}

void TestSha256Bytes() {
    // SHA-256 of empty string
    std::string empty = "";
    std::string result = DriverNest::Net::Sha256Bytes(
        std::span<const BYTE>(reinterpret_cast<const BYTE*>(empty.data()), empty.size()));
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    TEST_ASSERT_EQ(result, expected);
}

} // namespace

TEST(HashSha256KnownVector) { TestSha256KnownVector(); }
TEST(HashSha256Empty) { TestSha256Bytes(); }
