#pragma once
#include "DeviceInfo.h"
#include <vector>

namespace DriverNest::Hw {

// ---------------------------------------------------------------------------
// Enumerate present devices using SetupAPI and compute their driver state.
// ---------------------------------------------------------------------------
struct DeviceEnum {
    // Enumerate all present devices on the system. Each call performs a fresh
    // SetupDiGetClassDevs + SetupDiEnumDeviceInfo loop and CFGMGR queries.
    static std::vector<Device> EnumerateAll();

    // Refresh a single device by instance ID (lightweight re-query of problem
    // code and driver info only). Returns an empty device when not found.
    static Device RefreshOne(std::wstring_view instanceId);

    // Compare two enumerations to produce a concise delta useful for the UI.
    struct Delta {
        std::vector<Device> added;
        std::vector<Device> removed;
        std::vector<Device> changed;  // same instanceId, different problem/state
    };
    static Delta Diff(std::vector<Device> oldList, std::vector<Device> newList);
};

} // namespace DriverNest::Hw
