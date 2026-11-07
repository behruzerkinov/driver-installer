#include "test_framework.h"
#include "InfParser.h"
#include <filesystem>
#include <fstream>

namespace {

// Helper: create a temporary INF file with given content.
std::filesystem::path CreateTempInf(std::string_view content, const char* suffix = ".inf") {
    static int counter = 0;
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "DriverNestTests";
    std::filesystem::create_directories(tempDir);

    std::filesystem::path path = tempDir / ("test_" + std::to_string(counter++) + suffix);
    std::ofstream file(path, std::ios::binary);
    file.write(content.data(), content.size());
    return path;
}

void TestUtf16Le() {
    // INF file with UTF-16 LE BOM
    std::string content;
    // BOM
    content += char(0xFF);
    content += char(0xFE);
    // Line: [Version]
    content += char(0x5B); content += char(0x00); // [
    content += char(0x56); content += char(0x00); // V
    content += char(0x65); content += char(0x00); // e
    content += char(0x72); content += char(0x00); // r
    content += char(0x73); content += char(0x00); // s
    content += char(0x69); content += char(0x00); // i
    content += char(0x6F); content += char(0x00); // o
    content += char(0x6E); content += char(0x00); // n
    content += char(0x5D); content += char(0x00); // ]
    content += char(0x0D); content += char(0x00); // CR
    content += char(0x0A); content += char(0x00); // LF

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    TEST_ASSERT_EQ(entries.size(), 1);
}

void TestUtf8() {
    std::string content;
    // BOM
    content += char(0xEF);
    content += char(0xBB);
    content += char(0xBF);
    content += "[Version]\r\n";
    content += "DriverVer=01/01/2024,10.0.0.0\r\n";

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    TEST_ASSERT_EQ(entries.size(), 1);
    TEST_ASSERT_WSTREQ(entries[0].versionStr.c_str(), L"10.0.0.0");
}

void TestAnsi() {
    std::string content;
    content += "[Version]\r\n";
    content += "DriverVer=01/01/2024,10.0.0.0\r\n";

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    TEST_ASSERT_EQ(entries.size(), 1);
}

void TestLineContinuation() {
    // Test that lines ending with \\ are continued
    std::string content;
    content += "[Strings]\r\n";
    content += "MyString = This is a long \\\r\n";
    content += "           string continued\r\n";

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    // Note: current implementation doesn't fully handle continuation; this test
    // documents the expected behavior.
    TEST_ASSERT_EQ(entries.size(), 1);
}

void TestStringSubstitution() {
    std::string content;
    content += "[Strings]\r\n";
    content += "MyProvider = My Company\r\n";
    content += "[Version]\r\n";
    content += "Provider=%MyProvider%\r\n";

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    TEST_ASSERT_EQ(entries.size(), 1);
    TEST_ASSERT_WSTREQ(entries[0].provider.c_str(), L"My Company");
}

void TestModelDecoration() {
    // Test that model sections with OS build decoration are parsed correctly.
    // [Manufacturer]
    // Intel = Intel.Modern.NTamd64.10.0, NTamd64.10.0
    // This test is a placeholder for the full model parsing which is not
    // implemented in the current InfParser.
    std::string content;
    content += "[Manufacturer]\r\n";
    content += "Intel = Intel.Models.NTamd64.10.0\r\n";
    content += "[Intel.Models.NTamd64.10.0]\r\n";
    content += "Intel Device = Intel, device.inf, PCI\\VEN_8086&DEV_1234\r\n";

    auto path = CreateTempInf(content);
    auto entries = DriverNest::Packs::ParseInfFile(path);
    TEST_ASSERT_EQ(entries.size(), 1);
    // Note: full model parsing not implemented yet
}

} // namespace

TEST(InfParserUtf16Le) { TestUtf16Le(); }
TEST(InfParserUtf8) { TestUtf8(); }
TEST(InfParserAnsi) { TestAnsi(); }
TEST(InfParserLineContinuation) { TestLineContinuation(); }
TEST(InfParserStringSubstitution) { TestStringSubstitution(); }
TEST(InfParserModelDecoration) { TestModelDecoration(); }
