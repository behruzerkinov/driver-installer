#pragma once
#include "DeviceInfo.h"
#include <vector>
#include <string>
#include <windows.h>

namespace DriverNest::Ui {

enum class ItemInstallState {
    Idle,
    Queued,          // Waiting to install
    Downloading,     // Downloading %
    Installing,      // Installing via pnputil
    Verifying,       // Verifying driver
    Installed,       // Installed successfully
    PendingRestart,  // Installed in session, restart required
    Failed           // Error
};

struct ItemProgress {
    ItemInstallState state = ItemInstallState::Idle;
    int percent = 0;              // 0 - 100
    int64_t bytesCurrent = 0;
    int64_t bytesTotal = 0;
    std::wstring message;
};

class DeviceList {
public:
    static void Init(HWND hwnd);
    static void SetMainWindow(HWND hwndMain);

    // Replace the full device list
    static void SetDevices(HWND hwnd, std::vector<Hw::Device> devices);

    // Skeleton loading state
    static void SetScanning(HWND hwnd, bool scanning, float shimmerPhase = 0.0f);
    static bool IsScanning(HWND hwnd);

    // Selection methods
    static void SelectAll(HWND hwnd);
    static void DeselectAll(HWND hwnd);
    static void ToggleSelectAll(HWND hwnd);
    static bool AreAllSelected(HWND hwnd);
    static int GetSelectedCount(HWND hwnd);
    static void ClearSelection(HWND hwnd);

    // Return the list of selected instance IDs
    static std::vector<std::wstring> GetSelectedInstanceIds(HWND hwnd);
    static std::vector<Hw::Device> GetSelectedDevices(HWND hwnd);
    static bool SelectOnlyForInstall(HWND hwnd, std::wstring_view instanceId);

    // Retrieve the device at a given index
    static Hw::Device GetDeviceAt(HWND hwnd, int index);
    static int GetDeviceCount(HWND hwnd);
    static int FindMatchingDevice(HWND hwnd, std::wstring_view query);

    // Per-driver progress updates
    static void SetItemProgress(HWND hwnd, int index, const ItemProgress& progress);
    static void SetItemProgressByInstanceId(HWND hwnd, const std::wstring& instanceId, const ItemProgress& progress);
    static void ResetProgress(HWND hwnd);

    // Notification handling (custom draw, clicks, keyboard)
    static LRESULT HandleNotify(HWND hwnd, NMHDR* pnm);

    // Mouse / hit test helpers for custom checkboxes
    static bool HandleClick(HWND hwnd, int x, int y);
};

} // namespace DriverNest::Ui
