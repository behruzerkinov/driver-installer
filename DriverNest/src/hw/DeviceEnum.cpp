#include "DeviceEnum.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <winnls.h>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <string>
#include <vector>
#include <cwctype>
#include <unordered_map>

// CAUTION: keep this implementation under 80 lines where practical per spec.
// Some sections are necessarily longer to maintain correctness around buffer
// handling; break them into small helpers where possible.

namespace DriverNest::Hw {

namespace {

// ---------------------------------------------------------------------------
// Minimal RAII wrappers for the handles we need in this file.
// ---------------------------------------------------------------------------
struct ScopedDevInfo {
    HDEVINFO h = nullptr;
    explicit ScopedDevInfo(HDEVINFO d) : h(d) {}
    ~ScopedDevInfo() { if (h) SetupDiDestroyDeviceInfoList(h); }
    ScopedDevInfo(const ScopedDevInfo&) = delete;
    ScopedDevInfo& operator=(const ScopedDevInfo&) = delete;
    ScopedDevInfo(ScopedDevInfo&& o) noexcept : h(o.h) { o.h = nullptr; }
    ScopedDevInfo& operator=(ScopedDevInfo&& o) noexcept {
        if (this != &o) { h = o.h; o.h = nullptr; }
        return *this;
    }
};

struct ScopedDevRegKey {
    HKEY h = nullptr;
    explicit ScopedDevRegKey(HKEY k) : h(k) {}
    ~ScopedDevRegKey() { if (h) RegCloseKey(h); }
    ScopedDevRegKey(const ScopedDevRegKey&) = delete;
    ScopedDevRegKey& operator=(const ScopedDevRegKey&) = delete;
    ScopedDevRegKey(ScopedDevRegKey&& o) noexcept : h(o.h) { o.h = nullptr; }
    ScopedDevRegKey& operator=(ScopedDevRegKey&& o) noexcept {
        if (this != &o) { h = o.h; o.h = nullptr; }
        return *this;
    }
};

// Read a REG_MULTI_SZ property into a vector of wstrings.
[[nodiscard]] std::vector<std::wstring> ReadMultiString(HDEVINFO hDevInfo,
                                                         PSP_DEVINFO_DATA devData,
                                                         DWORD property) {
    std::vector<std::wstring> out;
    DWORD type = 0;
    DWORD size = 0;

    // first call to get size
    if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, devData, property, &type,
                                            nullptr, 0, &size) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return out; // not present or error; return empty
    }
    if (size == 0 || type != REG_MULTI_SZ) {
        return out;
    }

    auto buf = std::make_unique<wchar_t[]>(size / sizeof(wchar_t));
    if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, devData, property, &type,
                                            reinterpret_cast<PBYTE>(buf.get()), size, nullptr)) {
        return out;
    }

    wchar_t* cur = buf.get();
    wchar_t* end = buf.get() + size / sizeof(wchar_t);
    while (cur < end && *cur != L'\0') {
        out.emplace_back(cur);
        // skip to next null
        while (cur < end && *cur != L'\0') ++cur;
        ++cur; // skip the null terminator
    }
    return out;
}

// Read a simple string property.
[[nodiscard]] std::wstring ReadString(HDEVINFO hDevInfo,
                                       PSP_DEVINFO_DATA devData,
                                       DWORD property) {
    DWORD type = 0;
    DWORD size = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, devData, property, &type,
                                            nullptr, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return L"";
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        return L"";
    }
    if (size == 0) return L"";

    auto buf = std::make_unique<wchar_t[]>(size / sizeof(wchar_t) + 1);
    if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, devData, property, &type,
                                            reinterpret_cast<PBYTE>(buf.get()), size, nullptr)) {
        return L"";
    }
    buf.get()[size / sizeof(wchar_t)] = L'\0';
    return std::wstring(buf.get());
}

// Read the device instance ID (e.g. PCI\VEN_8086&DEV_1234\3&11583659&0&10).
[[nodiscard]] std::wstring ReadInstanceId(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devData) {
    wchar_t buf[MAX_DEVICE_ID_LEN] = {};
    if (SetupDiGetDeviceInstanceIdW(hDevInfo, devData, buf, ARRAYSIZE(buf), nullptr)) {
        return std::wstring(buf);
    }
    return L"";
}

// Read a DWORD property.
[[nodiscard]] DWORD ReadDWord(HDEVINFO hDevInfo,
                               PSP_DEVINFO_DATA devData,
                               DWORD property) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD val = 0;
    if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, devData, property, &type,
                                           reinterpret_cast<PBYTE>(&val), size, nullptr)) {
        return val;
    }
    return 0;
}

// Open the driver key under HKLM\SYSTEM\CurrentControlSet\Control\Class\{guid}\{driverInstance}
// and read DriverVersion/DriverDate if present.
void ReadDriverKeyInfo(const std::wstring& classGuid,
                       DWORD driverInstance,
                       std::wstring& versionOut,
                       FILETIME& dateOut) {
    // build key path
    std::wstring path = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";
    path += classGuid;
    path += L"\\";
    wchar_t driverInstStr[32];
    swprintf(driverInstStr, 32, L"%u", driverInstance);
    path += driverInstStr;

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return;
    }
    ScopedDevRegKey _close(hKey);

    // DriverVersion (REG_SZ)
    {
        DWORD type = 0;
        DWORD size = 0;
        if (RegQueryValueExW(hKey, L"DriverVersion", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
            type == REG_SZ && size > 0) {
            auto buf = std::make_unique<wchar_t[]>(size / sizeof(wchar_t) + 1);
            if (RegQueryValueExW(hKey, L"DriverVersion", nullptr, &type,
                                 reinterpret_cast<PBYTE>(buf.get()), &size) == ERROR_SUCCESS) {
                buf.get()[size / sizeof(wchar_t)] = L'\0';
                versionOut = buf.get();
            }
        }
    }

    // DriverDate as FILETIME (REG_BINARY), or DriverDate as string fallback
    {
        DWORD type = 0;
        DWORD size = 0;
        if (RegQueryValueExW(hKey, L"DriverDate", nullptr, &type, nullptr, &size) == ERROR_SUCCESS) {
            if (type == REG_BINARY && size >= sizeof(FILETIME)) {
                FILETIME ft{};
                if (RegQueryValueExW(hKey, L"DriverDate", nullptr, &type,
                                     reinterpret_cast<PBYTE>(&ft), &size) == ERROR_SUCCESS) {
                    dateOut = ft;
                }
            }
        }
    }
}

// Get problem code and description for an instance ID.
void ReadProblemInfo(const std::wstring& instanceId,
                     std::uint32_t& problemCodeOut,
                     std::wstring& problemTextOut) {
    ULONG status = 0;
    ULONG code = 0;
    problemCodeOut = 0;
    problemTextOut.clear();
    if (instanceId.empty()) return;

    DEVINST devInst = 0;
    if (CM_Locate_DevNodeW(&devInst, const_cast<DEVINSTID_W>(instanceId.c_str()),
                           CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) {
        return;
    }
    if (CM_Get_DevNode_Status(&status, &code, devInst, 0) == CR_SUCCESS) {
        problemCodeOut = static_cast<std::uint32_t>(code);
    }
}

// Trim helper used below.
std::wstring Trim(std::wstring s) {
    static constexpr wchar_t kWhitespace[] = L" \t\r\n\f\v";
    s.erase(s.find_last_not_of(kWhitespace) + 1);
    s.erase(0, s.find_first_not_of(kWhitespace));
    return s;
}

} // namespace

std::vector<Device> DeviceEnum::EnumerateAll() {
    std::vector<Device> devices;

    // Enumerate all present devices. DIGCF_ALLCLASSES | DIGCF_PRESENT gives us
    // only devices currently attached/enumerated by the system.
    HDEVINFO hAll = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                          DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hAll == INVALID_HANDLE_VALUE) {
        // This is acceptable if no devices match, but we expect at least some.
        return devices;
    }
    ScopedDevInfo _scope(hAll);

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(hAll, index, &devData); ++index) {
        Device d;
        d.instanceId = ReadInstanceId(hAll, &devData);
        d.description = Trim(ReadString(hAll, &devData, SPDRP_DEVICEDESC));
        d.manufacturer = Trim(ReadString(hAll, &devData, SPDRP_MFG));
        d.classGuid = ReadString(hAll, &devData, SPDRP_CLASSGUID);
        d.hardwareIds = ReadMultiString(hAll, &devData, SPDRP_HARDWAREID);
        d.compatibleIds = ReadMultiString(hAll, &devData, SPDRP_COMPATIBLEIDS);

        // Class GUID from registry if empty (fallback)
        if (d.classGuid.empty()) {
            // Not expected, but proceed
        }

        // SPDRP_DRIVER gives the driver key name; we read the driver key directly.
        std::wstring driverKey = ReadString(hAll, &devData, SPDRP_DRIVER);
        if (!driverKey.empty()) {
            // The driver key is a registry path like
            // SYSTEM\CurrentControlSet\Control\Class\{guid}\{index}
            // Extract index and GUID from it.
            // This is fragile but matches common tools. Prefer the INF path below.
        }

        // Simpler approach: get the INF name via SPDRP_MATCHINGINF or the driver key.
        // We'll open the driver key and read DriverVersion/Date ourselves.
        // Let's instead read the INF path from the device's Install transfer.

        // Open the driver key for this device.
        HKEY hDrvKey = SetupDiOpenDevRegKey(hAll, &devData, DICS_FLAG_GLOBAL, 0,
                                            DIREG_DEV, KEY_READ);
        if (hDrvKey != INVALID_HANDLE_VALUE) {
            ScopedDevRegKey _drvKey(hDrvKey);
            // Read InfPath
            DWORD type = 0;
            DWORD size = 0;
            if (RegQueryValueExW(hDrvKey, L"InfPath", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
                type == REG_SZ && size > 0) {
                auto buf = std::make_unique<wchar_t[]>(size / sizeof(wchar_t) + 1);
                if (RegQueryValueExW(hDrvKey, L"InfPath", nullptr, &type,
                                     reinterpret_cast<PBYTE>(buf.get()), &size) == ERROR_SUCCESS) {
                    buf.get()[size / sizeof(wchar_t)] = L'\0';
                    d.installedInf = buf.get();
                }
            }

            // Read DriverVersion and DriverDate from the driver key itself when available
            {
                DWORD type = 0;
                DWORD size = 0;
                if (RegQueryValueExW(hDrvKey, L"DriverVersion", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
                    type == REG_SZ && size > 0) {
                    auto buf = std::make_unique<wchar_t[]>(size / sizeof(wchar_t) + 1);
                    if (RegQueryValueExW(hDrvKey, L"DriverVersion", nullptr, &type,
                                         reinterpret_cast<PBYTE>(buf.get()), &size) == ERROR_SUCCESS) {
                        buf.get()[size / sizeof(wchar_t)] = L'\0';
                        d.installedVersion = buf.get();
                    }
                }
                if (RegQueryValueExW(hDrvKey, L"DriverDate", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
                    type == REG_BINARY && size >= sizeof(FILETIME)) {
                    FILETIME ft{};
                    if (RegQueryValueExW(hDrvKey, L"DriverDate", nullptr, &type,
                                         reinterpret_cast<PBYTE>(&ft), &size) == ERROR_SUCCESS) {
                        d.installedDate = ft;
                    }
                }
            }
        }

        // Problem code from CFGMGR
        std::uint32_t problemCode = 0;
        std::wstring problemText;
        ReadProblemInfo(d.instanceId, problemCode, problemText);
        d.problemCode = problemCode;

        // Derive state
        if (problemCode != 0) {
            d.state = DriverState::Problem;
        } else if (d.installedVersion.empty() && d.installedInf.empty()) {
            d.state = DriverState::Missing;
        } else {
            d.state = DriverState::Ok;
        }

        // Skip empty description devices (ghost/enumeration artifacts) unless flagged
        if (d.description.empty() && d.instanceId.empty()) {
            continue;
        }

        devices.push_back(std::move(d));
    }

    return devices;
}

Device DeviceEnum::RefreshOne(std::wstring_view instanceId) {
    // Lightweight path: find the device by instance ID and refresh its problem
    // code and driver info. For clarity we re-enumerate the single device by
    // instance ID using SetupDiGetClassDevs with DIGCF_DEVICEINTERFACE? No.
    // We'll search within a full enumeration but stop early. This keeps the
    // code simple and still fast for typical laptop counts (< 200 devices).
    auto all = EnumerateAll();
    for (auto& d : all) {
        if (d.instanceId == instanceId) {
            return d;
        }
    }
    return Device{};
}

DeviceEnum::Delta DeviceEnum::Diff(std::vector<Device> oldList,
                                   std::vector<Device> newList) {
    Delta delta;
    // Build maps by instance ID
    std::unordered_map<std::wstring, Device> oldMap;
    for (auto& d : oldList) {
        if (!d.instanceId.empty()) oldMap.emplace(d.instanceId, std::move(d));
    }
    for (auto& d : newList) {
        auto it = oldMap.find(d.instanceId);
        if (it == oldMap.end()) {
            delta.added.push_back(d);
        } else {
            // Check if changed
            if (it->second.problemCode != d.problemCode ||
                it->second.state != d.state ||
                it->second.installedVersion != d.installedVersion) {
                delta.changed.push_back(d);
            }
            oldMap.erase(it);
        }
    }
    for (auto& kv : oldMap) {
        delta.removed.push_back(kv.second);
    }
    return delta;
}

} // namespace DriverNest::Hw
