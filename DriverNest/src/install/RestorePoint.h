#pragma once
#include <windows.h>
#include <string>

namespace DriverNest::Install {

// ---------------------------------------------------------------------------
// System Restore point via SRSetRestorePointW (srclient.dll).
//
// The DLL is loaded at runtime, not linked statically, so the app still starts
// if the DLL is missing.
// ---------------------------------------------------------------------------
struct RESTOREPOINT_HANDLE {
    HRESULT hr = 0;
    void* pData = nullptr; // opaque handle
};

// Create a restore point of type DEVICE_DRIVER_INSTALL (10).
// Returns true on success; handle is valid until Close is called.
[[nodiscard]] bool Create(const std::wstring& description,
                          RESTOREPOINT_HANDLE& outHandle);

// Close the handle after the install completes.
void Close(RESTOREPOINT_HANDLE& handle);

} // namespace DriverNest::Install
