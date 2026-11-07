#include "RestorePoint.h"
#include "Log.h"
#include <windows.h>

namespace DriverNest::Install {

using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

namespace {

// Function pointer types for SRSetRestorePointW.
typedef HRESULT (WINAPI* SRSetRestorePointW_t)(
    RESTOREPOINT_HANDLE* pInfo,
    DWORD dwEventType,
    LPCWSTR pwszDescription,
    DWORD dwFlags);

typedef void (WINAPI* SRSetRestorePointClose_t)(
    RESTOREPOINT_HANDLE hBackup);

SRSetRestorePointW_t s_SRSetRestorePointW = nullptr;
SRSetRestorePointClose_t s_SRClose = nullptr;
HMODULE s_srclientDll = nullptr;

// Lazy-load srclient.dll.
bool LoadSrClient() {
    if (s_srclientDll) return true;

    s_srclientDll = LoadLibraryW(L"srclient.dll");
    if (!s_srclientDll) {
        LOG_WARN(L"Could not load srclient.dll; System Restore unavailable.");
        return false;
    }

    s_SRSetRestorePointW = reinterpret_cast<SRSetRestorePointW_t>(
        GetProcAddress(s_srclientDll, "SRSetRestorePointW"));
    s_SRClose = reinterpret_cast<SRSetRestorePointClose_t>(
        GetProcAddress(s_srclientDll, "SRSetRestorePointClose"));

    if (!s_SRSetRestorePointW || !s_SRClose) {
        LOG_WARN(L"Could not find SRSetRestorePointW in srclient.dll.");
        FreeLibrary(s_srclientDll);
        s_srclientDll = nullptr;
        return false;
    }

    return true;
}

} // namespace

bool Create(const std::wstring& description, RESTOREPOINT_HANDLE& outHandle) {
    if (!LoadSrClient()) {
        return false;
    }

    // Build the RESTOREPOINT_INFO structure
    // RESTOREPOINT_INFO is defined in srclient.h but we'll define it here
    // to avoid linking issues.
    struct RESTOREPOINT_INFO {
        DWORD cbSize;
        DWORD dwEventType;
        LPCWSTR pwszDescription;
        DWORD dwRestorePointDuration;
        DWORD dwFlags;
        HANDLE hToken;
    };

    RESTOREPOINT_INFO info{};
    info.cbSize = sizeof(info);
    info.dwEventType = 10; // DEVICE_DRIVER_INSTALL
    info.pwszDescription = description.c_str();
    info.dwRestorePointDuration = 0;
    info.dwFlags = 0; // SRL_IGNORE_SMARTSLEEP (0x00000001) - not set for now
    info.hToken = nullptr;

    // Begin the restore point
    HRESULT hr = s_SRSetRestorePointW(&outHandle, info.dwEventType,
                                      info.pwszDescription, info.dwFlags);
    if (FAILED(hr)) {
        LOG_ERROR(L"SRSetRestorePointW failed: 0x" +
                  std::to_wstring(static_cast<unsigned long long>(hr)));
        return false;
    }

    return true;
}

void Close(RESTOREPOINT_HANDLE& handle) {
    if (!s_SRClose) return;
    s_SRClose(handle);
    handle.hr = 0;
    handle.pData = nullptr;
}

} // namespace DriverNest::Install
