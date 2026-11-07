#pragma once
#include "DeviceInfo.h"
#include <vector>
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Device list control wrapper.
//
// Uses SysListView32 in report mode with:
//  - Groups by class (LVM_ENABLEGROUPVIEW)
//  - Checkboxes (LVS_EX_CHECKBOXES)
//  - Double buffer, full row select
//  - Virtual mode (LVS_OWNERDATA) for > 2000 devices
//  - Custom draw for status dot and row tint
// ---------------------------------------------------------------------------
class DeviceList {
public:
    // Initialize an existing SysListView32 HWND.
    static void Init(HWND hwnd);

    // Set the main window HWND (for keyboard hooks, settings, etc.).
    static void SetMainWindow(HWND hwndMain);

    // Replace the full device list. Triggers a rebuild of groups and items.
    static void SetDevices(HWND hwnd, std::vector<Hw::Device> devices);

    // Select all visible items.
    static void SelectAll(HWND hwnd);

    // Clear selection.
    static void ClearSelection(HWND hwnd);

    // Return the list of selected instance IDs.
    static std::vector<std::wstring> GetSelectedInstanceIds(HWND hwnd);

    // Retrieve the device at a given index (for details pane refresh).
    static Hw::Device GetDeviceAt(HWND hwnd, int index);

    // Handle NM_CUSTOMDRAW and other notifications from the list.
    static LRESULT HandleNotify(HWND hwnd, NMHDR* pnm);

};
// Per-HWND state lives in GWLP_USERDATA and is owned by DeviceList.cpp.

} // namespace DriverNest::Ui
