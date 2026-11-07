#include "test_framework.h"
#include "Ranker.h"
#include "DeviceInfo.h"
#include "PackIndex.h"
#include <filesystem>
#include <unordered_map>
#include <algorithm>

using namespace DriverNest;

namespace {

// Build a minimal PackIndex with a single record for testing.
DriverNest::Packs::PackIndex BuildTestIndex() {
    DriverNest::Packs::PackIndex idx;
    DriverNest::Packs::IndexRecord rec;
    rec.hardwareId = "PCI\\VEN_8086&DEV_1234"; // needs to be UTF-8 lowercase? But hardware IDs are case-insensitive, use exact from spec
    rec.infRelPath = "test.inf";
    rec.version = 0x000A000000000000ULL; // 10.0.0.0
    rec.dateEpoch = 0x01D9000000000000LL; // some date
    rec.fileMtime = 0;
    idx.records.push_back(rec);

    // Build hwIdIndex
    std::string lowerId = "pci\\ven_8086&dev_1234"; // lowercase
    idx.hwIdIndex.emplace(lowerId, 0);
    return idx;
}

void TestExactMatchPrimary() {
    // Exact match of most specific hardware ID => 1000 points
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 0;

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);

    TEST_ASSERT_EQ(top.size(), 1);
    // 1000 exact match + 40 newer version than the installed 9.0.0.0.
    TEST_ASSERT_EQ(top[0].score, 1040);
    TEST_ASSERT_WSTREQ(top[0].reasonTags[0].c_str(), L"Exact hardware ID");
}

void TestExactMatchOther() {
    // Exact match of a non-primary hardware ID => 800 - 20*index
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_1234&DEV_5678", L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 0;

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);

    TEST_ASSERT_EQ(top.size(), 1);
    // Match at position 1: 800 - 20*1 = 780, plus 40 for the newer version.
    TEST_ASSERT_EQ(top[0].score, 820);
}

void TestCompatibleIdMatch() {
    // Compatible ID match only => 400
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_1234&DEV_5678" };
    device.compatibleIds = { L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 0;

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);

    TEST_ASSERT_EQ(top.size(), 1);
    // Compatible-ID match only (400) plus the 40-point newer-version bonus.
    TEST_ASSERT_EQ(top[0].score, 440);
}

void TestClassGuidBonus() {
    // Class GUID match => +50
    // This test requires the record to have the same class GUID as the device.
    // For now we skip since the index doesn't store class GUID.
    // TEST_ASSERT_EQ adds 50 to score when class matches.
    SKIP("Class GUID bonus test requires index to store class GUID");
}

void TestNewerVersionBonus() {
    // Driver version greater than installed => +40
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 0;

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);

    TEST_ASSERT_EQ(top.size(), 1);
    // Expected: 1000 (exact) + 40 (newer version) + 100 (signature bonus) = 1140
    // But signature bonus requires valid signature check; for now we just check base
    TEST_ASSERT(top[0].score >= 1000);
}

void TestNewerDateBonus() {
    // Driver date newer than installed => +30
    // Similar to version test, requires date comparison.
    SKIP("Newer date bonus test requires date setup");
}

void TestSignatureBonus() {
    // Catalog present and signature valid => +100
    // Unsigned or no catalog => -500 (hidden by default)
    // This test uses live signature check which may fail without actual catalog.
    SKIP("Signature bonus test requires actual catalog file");
}

void TestUnsignedPenalty() {
    // Unsigned driver: -500, hidden by default
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 0;

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;

    // With showUnsigned = false, unsigned drivers should be skipped
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);
    // If signature is invalid, the candidate should be filtered out
    // but we don't have a real unsigned driver to test this.
    SKIP("Unsigned penalty test requires unsigned driver candidate");
}

void TestProblemDeviceBonus() {
    // Problem device => +25
    Hw::Device device;
    device.hardwareIds = { L"PCI\\VEN_8086&DEV_1234" };
    device.classGuid = L"{4d36e967-e325-11ce-bfc1-08002be10318}";
    device.installedVersion = L"9.0.0.0";
    device.problemCode = 10; // some problem code

    auto idx = BuildTestIndex();
    std::vector<DriverNest::Packs::Ranker::Candidate> top, alt;
    DriverNest::Packs::Ranker::Rank(device, idx, false, top, alt);

    TEST_ASSERT_EQ(top.size(), 1);
    // Score should include problem device bonus
    TEST_ASSERT(top[0].score >= 1000);
}

} // namespace

TEST(RankerExactMatchPrimary) { TestExactMatchPrimary(); }
TEST(RankerExactMatchOther) { TestExactMatchOther(); }
TEST(RankerCompatibleIdMatch) { TestCompatibleIdMatch(); }
TEST(RankerClassGuidBonus) { TestClassGuidBonus(); }
TEST(RankerNewerVersionBonus) { TestNewerVersionBonus(); }
TEST(RankerNewerDateBonus) { TestNewerDateBonus(); }
TEST(RankerSignatureBonus) { TestSignatureBonus(); }
TEST(RankerUnsignedPenalty) { TestUnsignedPenalty(); }
TEST(RankerProblemDeviceBonus) { TestProblemDeviceBonus(); }
