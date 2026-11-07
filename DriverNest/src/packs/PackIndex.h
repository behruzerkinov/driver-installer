#pragma once
#include "InfParser.h"
#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace DriverNest::Packs {

// ---------------------------------------------------------------------------
// Binary index for a driver pack.
//
// File format (index.dnidx):
//   Header:
//     magic:   4 bytes "DNIX"
//     version: uint32  (current format version = 1)
//     rootPath: null-terminated UTF-8 string (relative to archive? No, absolute path stored)
//     recordCount: uint32
//   Records (repeated):
//     hardwareIdLen: uint32  (byte length of UTF-8 hardware ID string)
//     hardwareId:    bytes [hardwareIdLen]
//     infRelPathLen: uint32
//     infRelPath:    bytes [infRelPathLen]  (relative path from pack root)
//     version:       uint64  (packed version: major<<48 | minor<<32 | build<<16 | revision)
//     dateEpoch:     int64   (FILETIME as int64)
//     fileMtime:     int64   (FILETIME of the INF file)
// ---------------------------------------------------------------------------
struct IndexHeader {
    char magic[4];          // "DNIX"
    uint32_t version = 1;
    uint32_t recordCount = 0;
    // Followed by root path (null-terminated UTF-8) and then records.
};

struct IndexRecord {
    std::string hardwareId;      // UTF-8, lowercase
    std::string infRelPath;      // relative path from pack root, UTF-8
    uint64_t version = 0;        // packed version
    int64_t dateEpoch = 0;       // FILETIME as int64
    int64_t fileMtime = 0;       // FILETIME of INF file
};

struct PackIndex {
    std::filesystem::path rootPath;
    std::vector<IndexRecord> records;
    // Hash map from lowercase hardware ID to list of record indexes.
    std::unordered_multimap<std::string, size_t> hwIdIndex;

    // Load an existing index from disk. Returns false if the file is missing,
    // corrupt, or from a different pack root.
    [[nodiscard]] bool Load(const std::filesystem::path& indexPath);

    // Save the current index to disk.
    bool Save(const std::filesystem::path& indexPath) const;

    // Rebuild the index by scanning the pack root. This replaces all records.
    void Rebuild(const std::filesystem::path& packRoot);

    // Update only records whose INF files have changed (by mtime comparison).
    void UpdateIncremental(const std::filesystem::path& packRoot);

    // Look up candidates for a hardware ID. Returns record indexes (unsorted).
    std::vector<size_t> Lookup(const std::string& hardwareId) const;

    // Version packing helpers.
    static uint64_t PackVersion(std::wstring_view ver);
    static std::wstring UnpackVersion(uint64_t packed);
};

// ---------------------------------------------------------------------------
// The canonical index file name.
// ---------------------------------------------------------------------------
inline constexpr const char* IndexFileName = "index.dnidx";

} // namespace DriverNest::Packs
