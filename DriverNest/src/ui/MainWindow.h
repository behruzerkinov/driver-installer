#pragma once
#include "Theme.h"
#include "Backdrop.h"
#include "DeviceInfo.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <memory>
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Main application window.
//
// Layout (top to bottom):
//   - Header bar (48px): logo, app name, search box, theme toggle, settings button.
//   - Toolbar row (40px): Scan, Install selected (N), Undo last batch, filter chips.
//   - Body: split container with device list (left ~62%) and details pane (right ~38%).
//   - Status bar (28px): pack root, indexed driver count, last scan time, current task.
//
// Minimum size: 980x620 DIP. Remembers size/position from settings.
// DPI changes handled via WM_DPICHANGED.
// ---------------------------------------------------------------------------
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    // Create and show the window. Restores geometry from settings.
    void Create(const App::Settings& settings);

    // HWND access for message routing.
    HWND Hwnd() const { return hwnd_; }

    // Message routing entry point used by App::WndProc.
    static LRESULT CALLBACK HandleMessage(HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam);

    // ----- UI actions (called from worker thread results via PostMessage) -----
    void SetDeviceList(std::vector<Hw::Device> devices);
    void SetStatusText(std::wstring_view text);
    void SetTaskText(std::wstring_view text);
    void EnableInstallButton(bool enabled);
    void EnableUndoButton(bool enabled);
    void UpdateInstallButtonCount(int count);

    // Notify main window that a scan is starting/finished.
    void SetScanning(bool scanning);

    // Window geometry persistence.
    void SaveGeometry();

    // Registered window class name
    static constexpr const wchar_t* WindowClass = L"DriverNestMainWndClass";

private:
    HWND hwnd_ = nullptr;

    // Sub-controls
    HWND hSearchEdit_ = nullptr;
    HWND hScanBtn_ = nullptr;
    HWND hInstallBtn_ = nullptr;
    HWND hUndoBtn_ = nullptr;
    HWND hSettingsBtn_ = nullptr;
    HWND hThemeToggle_ = nullptr;
    HWND hDeviceList_ = nullptr;
    HWND hDetailsPane_ = nullptr;
    HWND hSplitter_ = nullptr;
    HWND hStatusBar_ = nullptr;

    // Layout state
    int headerHeight_ = 48;
    int toolbarHeight_ = 40;
    int statusHeight_ = 28;
    int splitterPos_ = 0; // pixels from left edge of body container
    int splitterMinLeft_ = 360;
    int splitterMinRight_ = 280;

    // Theme toggle state: true = dark (when manually overridden),
    // false = follow system. When mode == System this is ignored.
    bool manualDark_ = false;

    // Message handlers
    LRESULT OnCreate(LPCREATESTRUCT pcs);
    LRESULT OnDestroy();
    LRESULT OnSize(UINT wParam, int cx, int cy);
    LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    LRESULT OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
    LRESULT OnCommand(WORD wNotifyCode, WORD wID, HWND hwndCtl);
    LRESULT OnNotify(HWND hwndCtl, LPARAM lParam);
    LRESULT OnEraseBkgnd(HDC hdc);
    LRESULT OnMouseMove(UINT wParam, LPARAM lParam);
    LRESULT OnLButtonDown(UINT wParam, LPARAM lParam);
    LRESULT OnLButtonUp(UINT wParam, LPARAM lParam);
    LRESULT OnLButtonDblClk(UINT wParam, LPARAM lParam);
    LRESULT OnPaint();
    LRESULT OnKeyDown(WPARAM wParam, LPARAM lParam);
    LRESULT OnSysKeyDown(WPARAM wParam, LPARAM lParam);
    LRESULT OnGetMinMaxInfo(LPMINMAXINFO pmmi);

    // Initialize child controls after window creation.
    void InitChildren();

    // Layout recalculation.
    void DoLayout(int cx, int cy);

    // Custom draw for the device list (NM_CUSTOMDRAW).
    void OnDeviceListCustomDraw(NMHDR* pnm);

    // Theme toggle
    void ToggleTheme();

    // Settings button
    void OpenSettings();

    // Refresh devices
    void RefreshRequested();

    // Install selected
    void InstallSelectedRequested();

    // Undo last batch
    void UndoRequested();

    // Start a device scan (spawn worker, not inline).
    void StartScan();

    // WPARAM/LPARAM helpers
    static int LoWord(LPARAM l) { return static_cast<int>(l & 0xFFFF); }
    static int HiWord(LPARAM l) { return static_cast<int>((l >> 16) & 0xFFFF); }
    static WORD LoWord(WPARAM w) { return static_cast<WORD>(w & 0xFFFF); }
    static WORD HiWord(WPARAM w) { return static_cast<WORD>((w >> 16) & 0xFFFF); }
};

} // namespace DriverNest::Ui
