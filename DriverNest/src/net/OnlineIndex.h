#pragma once

#include "hw/DeviceInfo.h"
#include <atomic>
#include <string>
#include <vector>

namespace DriverNest::Net {

struct OnlineDriverCandidate {
    std::wstring instanceId;
    std::wstring family;
    std::wstring version;
    std::wstring vendor;
    std::wstring date;
    std::wstring url;
};

// Looks up device hardware IDs in WinDriverIndex and returns vendor hosted
// download references. The caller owns threading and applies results on UI.
[[nodiscard]] std::vector<OnlineDriverCandidate> SearchOnlineIndex(
    const std::vector<Hw::Device>& devices, const std::atomic<bool>& cancel);

} // namespace DriverNest::Net
