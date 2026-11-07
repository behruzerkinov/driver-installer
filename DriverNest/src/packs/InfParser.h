#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <windows.h>

namespace DriverNest::Packs {

// ---------------------------------------------------------------------------
// Parsed representation of a single INF file.
// ---------------------------------------------------------------------------
struct InfEntry {
    std::filesystem::path path;            // absolute path to the INF file
    std::wstring provider;                  // from [Version] Provider=
    std::wstring version;                   // from DriverVer=MM/DD/YYYY,version
    std::wstring versionStr;                // just the version component "x.y.z.w"
    FILETIME dateTime{};                     // from DriverVer date component
    std::int64_t dateEpoch = 0;             // FILETIME as int64 for comparison
    std::wstring classGuid;                 // from [Version] ClassGuid=
    std::wstring className;                 // from [Version] Class=
    std::wstring catalogFile;               // from [Version] CatalogFile=
    std::wstring manufacturerSection;       // e.g. "Intel" from [Manufacturer]
    // Models: each model line (section_name.NTamd64.build) with hardware IDs.
    struct Model {
        std::wstring sectionName;          // e.g. "Intel.NTamd64"
        std::wstring decoration;           // e.g. ".10.0" or empty
        int osBuild = 0;                   // the highest build number <= running OS
        std::vector<std::wstring> hardwareIds;
        std::vector<std::wstring> compatibleIds;
        std::wstring description;          // the description field
    };
    std::vector<Model> models;

    // All hardware match candidates extracted from models.
    struct HardwareMatch {
        std::wstring hardwareId;
        std::wstring compatibleId;  // may be empty if none
        std::wstring sourceSection;
    };
    std::vector<HardwareMatch> hardwareMatches;
};

// ---------------------------------------------------------------------------
// Parse an INF file.
//
// Returns an empty vector when the file cannot be parsed (bad encoding,
// exceeds size limits, malformed).
//
// Limits: file size 16 MB, line length 8192 chars. Exceeding files are
// rejected and logged.
// ---------------------------------------------------------------------------
[[nodiscard]] std::vector<InfEntry> ParseInfFile(const std::filesystem::path& path);

// ---------------------------------------------------------------------------
// Encoding detection via BOM.
// ---------------------------------------------------------------------------
enum class InfEncoding {
    Utf16Le,
    Utf16Be,
    Utf8,
    Ansi,   // Windows-1252 fallback
};

[[nodiscard]] InfEncoding DetectInfEncoding(std::filesystem::path path);

} // namespace DriverNest::Packs
