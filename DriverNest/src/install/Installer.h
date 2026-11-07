#pragma once
#include "DeviceInfo.h"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace DriverNest::Install {

// ---------------------------------------------------------------------------
// Driver installation via pnputil or UpdateDriverForPlugAndPlayDevices.
// ---------------------------------------------------------------------------
struct Installer {
    // Install a single driver package from an INF path.
    // Returns the published name (oemNNN.inf) on success, empty on failure.
    // Throws std::runtime_error on failure with error code in message.
    static std::wstring InstallInf(const std::filesystem::path& infPath,
                                   bool force = false);

    // Install multiple drivers sequentially. Calls progressCallback for each
    // step (restore point, each driver install, verification).
    // The callback receives (stepName, current, total).
    // Returns list of results (one per input INF).
    struct InstallResult {
        std::filesystem::path infPath;
        std::wstring publishedName;   // empty on failure
        bool success = false;
        std::wstring error;
        Hw::Device deviceAfter;       // refreshed device state after install
    };

    static std::vector<InstallResult> InstallBatch(
        const std::vector<std::filesystem::path>& infPaths,
        const std::function<void(std::wstring_view, int, int)>& progressCallback);

    // Rollback: reinstall a previous INF package.
    static bool RollbackInf(const std::filesystem::path& previousInfPath);

    // Clean up old/unused driver packages.
    static std::vector<std::wstring> CleanOldPackages();

    // Enumerates all third-party driver packages currently in the driver store.
    static std::vector<std::pair<std::wstring, std::wstring>> ListDriverStore();
};

} // namespace DriverNest::Install
