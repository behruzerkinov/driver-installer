#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace DriverNest::Hw {

// ---------------------------------------------------------------------------
// Driver state classification for a device.
// ---------------------------------------------------------------------------
enum class DriverState : std::int32_t {
    Ok = 0,            // Installed driver is present and no problem code.
    Missing,          // No driver installed (DriverDate/Version absent) but device present.
    Outdated,         // Installed driver exists but a newer candidate is available.
    Problem,          // Device has a non-zero problem code (Device Manager code).
    Unsigned,         // Installed or candidate driver is not signed or lacks a catalog.
    PendingRestart,   // Installed in session; reboot required to load and apply driver.
};

// ---------------------------------------------------------------------------
// Device descriptor produced by SetupAPI enumeration and CFGMGR queries.
// ---------------------------------------------------------------------------
struct Device {
    std::wstring instanceId;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring classGuid;           // GUID string, e.g. "{4d36e967-e325-11ce-bfc1-08002be10318}"
    std::vector<std::wstring> hardwareIds;    // most specific first
    std::vector<std::wstring> compatibleIds;
    std::wstring installedInf;        // path to the INF currently applied, may be empty
    std::wstring installedVersion;    // e.g. "10.0.22621.1" or empty
    FILETIME     installedDate{};     // from DriverDate registry value
    std::uint32_t problemCode = 0;     // CFGMGR problem code, 0 = no problem
    DriverState  state = DriverState::Ok;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// Convert FILETIME to a comparable int64 (100-nanosecond since 1601-01-01).
inline std::int64_t FileTimeToEpoch(std::uint32_t low, std::uint32_t high) noexcept {
    ULARGE_INTEGER u;
    u.LowPart  = low;
    u.HighPart = high;
    return static_cast<std::int64_t>(u.QuadPart);
}

inline std::int64_t FileTimeToEpoch(const FILETIME& ft) noexcept {
    return FileTimeToEpoch(ft.dwLowDateTime, ft.dwHighDateTime);
}

// Simple version string comparison for "major.minor.build.revision" style values
// used by driver packages. Returns -1/0/1 like strcmp semantics.
int CompareVersion(std::wstring_view a, std::wstring_view b) noexcept;

} // namespace DriverNest::Hw

// Re-export helpers for use by other namespaces (packs, install, etc.)
namespace DriverNest {
using Hw::FileTimeToEpoch;
using Hw::CompareVersion;
}
