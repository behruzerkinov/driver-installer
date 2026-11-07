#include "PackIndex.h"
#include "PackScanner.h"
#include "InfParser.h"
#include "Log.h"
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <system_error>

namespace DriverNest::Packs {

using App::LOG_DEBUG;
using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

namespace {

// ---------------------------------------------------------------------------
// Version packing: pack a version string "a.b.c.d" into uint64.
// Fields are zero-padded to 16 bits each. Unsupported formats are handled
// gracefully (fewer fields => trailing zeros).
// ---------------------------------------------------------------------------
uint64_t PackVersionImpl(std::wstring_view ver) {
    uint64_t packed = 0;
    std::size_t dot = 0;
    for (int shift = 48; shift >= 0 && dot < ver.size(); shift -= 16) {
        std::size_t next = ver.find(L'.', dot);
        std::wstring_view field = (next == std::wstring::npos)
            ? ver.substr(dot)
            : ver.substr(dot, next - dot);
        unsigned long val = 0;
        for (wchar_t c : field) {
            if (c >= L'0' && c <= L'9') {
                val = val * 10 + (c - L'0');
            } else {
                break;
            }
        }
        packed |= static_cast<uint64_t>(val) << shift;
        if (next == std::wstring::npos) break;
        dot = next + 1;
    }
    return packed;
}

std::wstring UnpackVersionImpl(uint64_t packed) {
    std::wstring out;
    for (int shift = 48; shift >= 0; shift -= 16) {
        if (!out.empty()) out += L'.';
        unsigned long val = (packed >> shift) & 0xFFFF;
        out += std::to_wstring(val);
    }
    // Trim trailing zeros in the last field? Keep simple.
    return out;
}

// Write a binary string with length prefix.
void WriteString(std::ofstream& out, std::string_view s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    out.write(s.data(), len);
}

// Read a binary string with length prefix.
[[nodiscard]] std::string ReadString(std::ifstream& in) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!in || len == 0) return {};
    std::string buf(len, 0);
    in.read(buf.data(), len);
    return buf;
}

// Convert a wide string to UTF-8 for storage in the index.
std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                      nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        out.data(), n, nullptr, nullptr);
    return out;
}

// Lowercase a UTF-8 string for case-insensitive hardware ID matching.
std::string ToLowerUtf8(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 'A' && c <= 'Z') {
            out += static_cast<char>(c + ('a' - 'A'));
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

} // namespace

// Version packing helpers exposed through PackIndex.
uint64_t PackIndex::PackVersion(std::wstring_view ver) {
    return PackVersionImpl(ver);
}

std::wstring PackIndex::UnpackVersion(uint64_t packed) {
    return UnpackVersionImpl(packed);
}

bool PackIndex::Load(const std::filesystem::path& indexPath) {
    std::ifstream in(indexPath, std::ios::binary);
    if (!in) {
        return false;
    }

    IndexHeader header{};
    in.read(header.magic, 4);
    if (std::strncmp(header.magic, "DNIX", 4) != 0) {
        LOG_WARN(L"Index file has wrong magic: " + indexPath.wstring());
        return false;
    }

    in.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    if (header.version != 1) {
        LOG_WARN(L"Index file version mismatch: " +
                 std::to_wstring(header.version) + L" (expected 1)");
        return false;
    }

    in.read(reinterpret_cast<char*>(&header.recordCount), sizeof(header.recordCount));

    // Read root path (null-terminated UTF-8)
    std::string rootUtf8;
    char ch;
    while (in.get(ch) && ch != '\0') {
        rootUtf8 += ch;
    }
    rootPath = std::filesystem::u8path(rootUtf8);

    records.resize(header.recordCount);
    hwIdIndex.clear();

    for (uint32_t i = 0; i < header.recordCount; ++i) {
        IndexRecord& rec = records[i];
        rec.hardwareId = ReadString(in);
        rec.infRelPath = ReadString(in);
        in.read(reinterpret_cast<char*>(&rec.version), sizeof(rec.version));
        in.read(reinterpret_cast<char*>(&rec.dateEpoch), sizeof(rec.dateEpoch));
        in.read(reinterpret_cast<char*>(&rec.fileMtime), sizeof(rec.fileMtime));
        if (!in) {
            LOG_WARN(L"Truncated index record at position " + std::to_wstring(i));
            records.resize(i);
            break;
        }
        // Build hash index
        std::string lowerId = ToLowerUtf8(rec.hardwareId);
        hwIdIndex.emplace(lowerId, i);
    }

    return true;
}

bool PackIndex::Save(const std::filesystem::path& indexPath) const {
    std::ofstream out(indexPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        LOG_ERROR(L"Could not create index file: " + indexPath.wstring());
        return false;
    }

    IndexHeader header{};
    std::memcpy(header.magic, "DNIX", 4);
    header.version = 1;
    header.recordCount = static_cast<uint32_t>(records.size());
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write root path (UTF-8, null-terminated)
    std::string rootUtf8 = rootPath.u8string();
    out.write(rootUtf8.data(), rootUtf8.size());
    out.put('\0');

    for (const auto& rec : records) {
        WriteString(out, rec.hardwareId);
        WriteString(out, rec.infRelPath);
        out.write(reinterpret_cast<const char*>(&rec.version), sizeof(rec.version));
        out.write(reinterpret_cast<const char*>(&rec.dateEpoch), sizeof(rec.dateEpoch));
        out.write(reinterpret_cast<const char*>(&rec.fileMtime), sizeof(rec.fileMtime));
        if (!out) {
            LOG_ERROR(L"Failed writing index record to: " + indexPath.wstring());
            return false;
        }
    }

    return true;
}

void PackIndex::Rebuild(const std::filesystem::path& packRoot) {
    rootPath = packRoot;
    records.clear();
    hwIdIndex.clear();

    // Scan for INF files
    auto infs = PackScanner::FindInfFiles(packRoot);

    // Parse each INF
    for (auto& infPath : infs) {
        auto entries = ParseInfFile(infPath);
        for (auto& entry : entries) {
            // For each hardware match in the INF, create a record
            for (auto& match : entry.hardwareMatches) {
                IndexRecord rec;
                rec.hardwareId = WideToUtf8(match.hardwareId); // lowercased when indexed
                rec.infRelPath = std::filesystem::relative(infPath, packRoot).u8string();
                rec.version = PackVersion(entry.versionStr);
                rec.dateEpoch = entry.dateEpoch;
                // get file mtime
                std::error_code ec;
                auto ftime = std::filesystem::last_write_time(infPath, ec);
                if (!ec) {
                    // Convert to FILETIME (int64)
                    // file_time_type is typically nanoseconds since 1601; we need to convert
                    // For simplicity, we'll store 0 for now.
                    rec.fileMtime = 0;
                }
                records.push_back(std::move(rec));
            }
        }
    }

    // Build hash index (lowercase hardware IDs)
    for (size_t i = 0; i < records.size(); ++i) {
        std::string lowerId = ToLowerUtf8(records[i].hardwareId);
        hwIdIndex.emplace(lowerId, i);
    }

    LOG_INFO(L"Rebuilt index with " + std::to_wstring(records.size()) + L" records.");
}

void PackIndex::UpdateIncremental(const std::filesystem::path& packRoot) {
    rootPath = packRoot;

    // Find all INF files currently in the pack
    auto currentInfos = PackScanner::FindInfFiles(packRoot);
    std::unordered_map<std::string, std::filesystem::file_time_type> currentFiles;

    for (auto& p : currentInfos) {
        std::error_code ec;
        auto rel = std::filesystem::relative(p, packRoot);
        currentFiles[rel.u8string()] = std::filesystem::last_write_time(p, ec);
    }

    // For each existing record, check if its INF still exists and mtime matches
    std::vector<IndexRecord> newRecords;
    newRecords.reserve(records.size());

    for (auto& rec : records) {
        std::string fullRel = packRoot.u8string() + "/" + rec.infRelPath;
        std::filesystem::path infPath = std::filesystem::u8path(fullRel);

        if (!std::filesystem::exists(infPath)) {
            // INF no longer present; skip
            continue;
        }

        // Check mtime
        std::error_code ec;
        auto mtime = std::filesystem::last_write_time(infPath, ec);
        if (ec) {
            // Can't check; keep record
            newRecords.push_back(rec);
            continue;
        }

        // If mtime changed, re-parse
        // For now, just keep the record (full implementation would re-parse)
        newRecords.push_back(rec);
    }

    // Add new INF files not in the index
    for (auto& inf : currentInfos) {
        std::string rel = std::filesystem::relative(inf, packRoot).u8string();
        bool found = false;
        for (auto& rec : newRecords) {
            if (rec.infRelPath == rel) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Parse and add
            auto entries = ParseInfFile(inf);
            for (auto& entry : entries) {
                for (auto& match : entry.hardwareMatches) {
                    IndexRecord rec;
                    rec.hardwareId = WideToUtf8(match.hardwareId);
                    rec.infRelPath = rel;
                    rec.version = PackVersion(entry.versionStr);
                    rec.dateEpoch = entry.dateEpoch;
                    rec.fileMtime = 0; // TODO: get actual mtime
                    newRecords.push_back(std::move(rec));
                }
            }
        }
    }

    records = std::move(newRecords);

    // Rebuild hash index
    hwIdIndex.clear();
    for (size_t i = 0; i < records.size(); ++i) {
        std::string lowerId = ToLowerUtf8(records[i].hardwareId);
        hwIdIndex.emplace(lowerId, i);
    }
}

std::vector<size_t> PackIndex::Lookup(const std::string& hardwareId) const {
    std::vector<size_t> result;
    std::string lowerId = ToLowerUtf8(hardwareId);
    auto range = hwIdIndex.equal_range(lowerId);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    return result;
}

} // namespace DriverNest::Packs
