#pragma once
#include "Theme.h"
#include "Backdrop.h"
#include "DeviceInfo.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <windows.h>

namespace DriverNest::Ui {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    void Create(const App::Settings& settings);
    HWND Hwnd() const { return hwnd_; }

    static LRESULT CALLBACK HandleMessage(HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam);

    void SetDeviceList(std::vector<Hw::Device> devices);
    void SetStatusText(std::wstring_view text);
    void SetTaskText(std::wstring_view text);
    void EnableInstallButton(bool enabled);
    void EnableUndoButton(bool enabled);
    void UpdateInstallButtonCount(int count);
    void SetScanning(bool scanning);
    void SaveGeometry();

    static constexpr const wchar_t* WindowClass = L"DriverNestMainWndClass";

private:
    HWND hwnd_ = nullptr;

    // Sub-controls
    HWND hSearchEdit_ = nullptr;
    HWND hScanBtn_ = nullptr;
    HWND hSelectAllBtn_ = nullptr;
    HWND hInstallBtn_ = nullptr;
    HWND hUndoBtn_ = nullptr;
    HWND hSettingsBtn_ = nullptr;
    HWND hThemeToggle_ = nullptr;
    HWND hDeviceList_ = nullptr;
    HWND hDetailsPane_ = nullptr;
    HWND hSplitter_ = nullptr;
    HWND hStatusBar_ = nullptr;

    // In-app progress panel
    HWND hProgressPanel_ = nullptr;
    HWND hProgressText_ = nullptr;
    HWND hProgressBar_ = nullptr;
    HWND hProgressCancel_ = nullptr;

    // Layout state
    int headerHeight_ = 54;
    int toolbarHeight_ = 44;
    int statusHeight_ = 28;
    int progressHeight_ = 38;
    bool showProgressPanel_ = false;

    int splitterPos_ = 0;
    int splitterMinLeft_ = 320;
    int splitterMinRight_ = 300;
    bool isDraggingSplitter_ = false;

    bool manualDark_ = false;
    bool isScanning_ = false;
    float shimmerPhase_ = 0.0f;

    std::atomic<bool> isInstalling_{false};
    std::atomic<bool> cancelInstall_{false};

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
    LRESULT OnPaint();
    LRESULT OnDrawItem(UINT idCtl, LPDRAWITEMSTRUCT pdis);
    LRESULT OnCtlColorEdit(HDC hdc, HWND hwndCtl);
    LRESULT OnCtlColorStatic(HDC hdc, HWND hwndCtl);
    LRESULT OnSetCursor(HWND hwndCursor, UINT hitTest, UINT message);
    LRESULT OnTimer(UINT_PTR nIDEvent);
    LRESULT OnKeyDown(WPARAM wParam, LPARAM lParam);
    LRESULT OnGetMinMaxInfo(LPMINMAXINFO pmmi);

    void InitChildren();
    void DoLayout(int cx, int cy);
    void ToggleTheme();
    void OpenSettings();
    void RefreshRequested();
    void InstallSelectedRequested();
    void UndoRequested();
    void StartScan();

    void RunInstallPipeline();
    void PromptRescanDialog();

    static int LoWord(LPARAM l) { return static_cast<int>(l & 0xFFFF); }
    static int HiWord(LPARAM l) { return static_cast<int>((l >> 16) & 0xFFFF); }
    static WORD LoWord(WPARAM w) { return static_cast<WORD>(w & 0xFFFF); }
    static WORD HiWord(WPARAM w) { return static_cast<WORD>((w >> 16) & 0xFFFF); }
};

} // namespace DriverNest::Ui
