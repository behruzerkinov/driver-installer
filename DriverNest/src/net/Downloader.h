#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <atomic>

namespace DriverNest::Net {

// ---------------------------------------------------------------------------
// Download a file from a URL using WinHTTP with TLS 1.2, progress, cancel.
// ---------------------------------------------------------------------------
struct DownloadOptions {
    std::wstring url;
    std::filesystem::path destination;  // final file path (without .part)
    std::wstring expectedSha256;        // lowercase hex, empty = skip verification
    std::function<void(int64_t current, int64_t total, int64_t speed)> progress;
    std::atomic<bool>* cancelFlag = nullptr;
};

// Result of a download operation.
struct DownloadResult {
    bool success = false;
    std::wstring error;
    std::filesystem::path finalPath;
};

// Download a file. Returns the result.
[[nodiscard]] DownloadResult DownloadFile(const DownloadOptions& options);

// Parse a minimal JSON manifest (hand-rolled parser, no external deps).
// Expected format:
// {
//   "hardwareIds": ["PCI\\VEN_8086&DEV_...", ...],
//   "url": "https://...",
//   "sha256": "abc123...",
//   "size": 123456,
//   "signer": "Microsoft Windows"
// }
struct ManifestEntry {
    std::vector<std::wstring> hardwareIds;
    std::wstring url;
    std::wstring sha256;
    int64_t size = 0;
    std::wstring signer;
};

[[nodiscard]] std::vector<ManifestEntry> ParseManifest(std::string_view json);

} // namespace DriverNest::Net
