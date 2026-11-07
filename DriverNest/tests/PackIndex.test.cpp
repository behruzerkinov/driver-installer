#include "test_framework.h"
#include "PackIndex.h"
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path CreateTempFile(std::string_view content) {
    static int counter = 0;
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "DriverNestTests";
    std::filesystem::create_directories(tempDir);
    std::filesystem::path path = tempDir / ("test_" + std::to_string(counter++) + ".dnidx");
    std::ofstream file(path, std::ios::binary);
    file.write(content.data(), content.size());
    return path;
}

void TestRoundTrip() {
    // Create an index, save it, load it back, verify.
    DriverNest::Packs::PackIndex idx;
    idx.rootPath = std::filesystem::path(L"C:\\TestPack");

    DriverNest::Packs::IndexRecord rec1;
    rec1.hardwareId = "PCI\\VEN_8086&DEV_1234";
    rec1.infRelPath = "drivers\\net\\test.inf";
    rec1.version = 0x000A000000000000ULL;
    rec1.dateEpoch = 1234567890LL;
    rec1.fileMtime = 0;
    idx.records.push_back(rec1);

    DriverNest::Packs::IndexRecord rec2;
    rec2.hardwareId = "USB\\VID_1234&PID_5678";
    rec2.infRelPath = "drivers\\usb\\test.inf";
    rec2.version = 0x000B000000000000ULL;
    rec2.dateEpoch = 2345678901LL;
    rec2.fileMtime = 0;
    idx.records.push_back(rec2);

    auto tempPath = CreateTempFile(std::string("")); // placeholder
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);

    bool saved = idx.Save(tempPath);
    TEST_ASSERT(saved);

    DriverNest::Packs::PackIndex loaded;
    bool loadedOk = loaded.Load(tempPath);
    TEST_ASSERT(loadedOk);

    TEST_ASSERT_EQ(loaded.records.size(), 2);
    TEST_ASSERT_EQ(loaded.records[0].hardwareId, "PCI\\VEN_8086&DEV_1234");
    TEST_ASSERT_EQ(loaded.records[1].hardwareId, "USB\\VID_1234&PID_5678");
    TEST_ASSERT_EQ(loaded.rootPath, std::filesystem::path(L"C:\\TestPack"));

    std::filesystem::remove(tempPath, ec);
}

void WriteSampleInf(const std::filesystem::path& infPath, const char* hwid) {
    std::ofstream file(infPath);
    file << "[Version]\r\n";
    file << "Signature=\"$WINDOWS NT$\"\r\n";
    file << "Class=Net\r\n";
    file << "ClassGuid={4d36e972-e325-11ce-bfc1-08002be10318}\r\n";
    file << "Provider=%Vendor%\r\n";
    file << "DriverVer=01/01/2024,1.0.0.0\r\n";
    file << "\r\n";
    file << "[Manufacturer]\r\n";
    file << "%Vendor%=Vendor,NTamd64\r\n";
    file << "\r\n";
    file << "[Vendor.NTamd64]\r\n";
    file << "%DeviceDesc% = Install, " << hwid << "\r\n";
    file << "\r\n";
    file << "[Strings]\r\n";
    file << "Vendor = \"Test Vendor\"\r\n";
    file << "DeviceDesc = \"Test Device\"\r\n";
}

void TestIncrementalUpdate() {
    // Rebuild an index over a pack folder, then pick up a newly added INF.
    DriverNest::Packs::PackIndex idx;
    idx.rootPath = std::filesystem::temp_directory_path() / "DriverNestTestPackUpdate";
    std::error_code ec;
    std::filesystem::remove_all(idx.rootPath, ec);
    std::filesystem::create_directories(idx.rootPath);

    WriteSampleInf(idx.rootPath / "test.inf", "PCI\\VEN_8086&DEV_1234");

    idx.Rebuild(idx.rootPath);
    TEST_ASSERT_EQ(idx.records.size(), 1);
    TEST_ASSERT_EQ(idx.records[0].hardwareId, "PCI\\VEN_8086&DEV_1234");

    // Adding a second package must be picked up by an incremental update.
    WriteSampleInf(idx.rootPath / "test2.inf", "PCI\\VEN_8086&DEV_5678");
    idx.UpdateIncremental(idx.rootPath);
    TEST_ASSERT_EQ(idx.records.size(), 2);

    // And a lookup for the new ID resolves to a record.
    TEST_ASSERT_EQ(idx.Lookup("PCI\\VEN_8086&DEV_5678").size(), 1);

    std::filesystem::remove_all(idx.rootPath, ec);
}

void TestCorruptHeader() {
    // Create a file with wrong magic and try to load it.
    auto tempPath = CreateTempFile(std::string("BADD") + "\x01\x00\x00\x00"); // version 1
    DriverNest::Packs::PackIndex idx;
    bool loaded = idx.Load(tempPath);
    TEST_ASSERT(!loaded);

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
}

void TestHardwareIdLookup() {
    DriverNest::Packs::PackIndex idx;
    idx.rootPath = std::filesystem::path(L"C:\\Test");

    DriverNest::Packs::IndexRecord rec;
    rec.hardwareId = "PCI\\VEN_8086&DEV_1234";
    rec.infRelPath = "test.inf";
    rec.version = 0;
    rec.dateEpoch = 0;
    rec.fileMtime = 0;
    idx.records.push_back(rec);

    // Build hash index manually (normally done in Load/Rebuild)
    std::string lowerId = "pci\\ven_8086&dev_1234";
    idx.hwIdIndex.emplace(lowerId, 0);

    auto results = idx.Lookup("PCI\\VEN_8086&DEV_1234");
    TEST_ASSERT_EQ(results.size(), 1);
    TEST_ASSERT_EQ(results[0], 0);

    // Lookup with different case should still work (since we lowercase both)
    auto results2 = idx.Lookup("pci\\ven_8086&dev_1234");
    TEST_ASSERT_EQ(results2.size(), 1);
}

void TestHardwareIdLookupNotFound() {
    DriverNest::Packs::PackIndex idx;
    auto results = idx.Lookup("NONEXISTENT");
    TEST_ASSERT_EQ(results.size(), 0);
}

} // namespace

TEST(PackIndexRoundTrip) { TestRoundTrip(); }
TEST(PackIndexIncrementalUpdate) { TestIncrementalUpdate(); }
TEST(PackIndexCorruptHeader) { TestCorruptHeader(); }
TEST(PackIndexHardwareIdLookup) { TestHardwareIdLookup(); }
TEST(PackIndexHardwareIdNotFound) { TestHardwareIdLookupNotFound(); }
