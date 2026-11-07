#include "MainWindow.h"
#include "Resources.h"
#include "DeviceList.h"
#include "DetailsPane.h"
#include "Dialogs.h"
#include "Strings.h"
#include "DeviceEnum.h"
#include "app/App.h"
#include "app/Settings.h"
#include "app/Log.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <vsstyle.h>
#include <winuser.h>
#include <shellapi.h>
#include <vector>
#include <algorithm>
#include <memory>

namespace DriverNest::Ui {

using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

// ---------------------------------------------------------------------------
// Window class registration and globals
// ---------------------------------------------------------------------------
struct WndClass {
    WndClass() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = MainWindow::HandleMessage;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // we paint ourselves
        wc.lpszClassName = MainWindow::WindowClass;
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1)); // app.ico id=1
        RegisterClassExW(&wc);
    }
};
static WndClass g_wndClass;

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------
MainWindow::MainWindow() = default;
MainWindow::~MainWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void MainWindow::Create(const App::Settings& settings) {
    // Ensure minimum size is respected when restoring geometry
    int width = settings.mainWidth;
    int height = settings.mainHeight;
    if (width < 980) width = 980;
    if (height < 620) height = 620;

    RECT desired{};
    desired.left = settings.mainLeft;
    desired.top = settings.mainTop;
    desired.right = desired.left + width;
    desired.bottom = desired.top + height;

    // Constrain to visible desktop
    HMONITOR hmon = MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hmon, &mi)) {
        RECT& rc = mi.rcWork;
        if (desired.left < rc.left) desired.left = rc.left;
        if (desired.top < rc.top) desired.top = rc.top;
        if (desired.right > rc.right) desired.right = rc.right;
        if (desired.bottom > rc.bottom) desired.bottom = rc.bottom;
    }

    int cx = desired.right - desired.left;
    int cy = desired.bottom - desired.top;

    hwnd_ = CreateWindowExW(
        0,
        MainWindow::WindowClass,
        Strings::WindowTitle.data(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        desired.left, desired.top, cx, cy,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        this // lpParam passed to WM_CREATE
    );

    if (!hwnd_) {
        LOG_ERROR(L"Failed to create main window.");
        return;
    }

    // DPI awareness: we depend on PerMonitorV2 from the manifest.
    // WM_DPICHANGED will be sent on DPI changes; we handle it.

    ShowWindow(hwnd_, settings.mainMaximized ? SW_MAXIMIZE : SW_SHOW);
    UpdateWindow(hwnd_);
}

// Static message dispatch table. All messages go through this.
LRESULT CALLBACK MainWindow::HandleMessage(HWND hwnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (uMsg == WM_NCCREATE) {
        // Get the 'this' pointer from CREATESTRUCT. Child windows are created
        // later in WM_CREATE, so just continue creation here (TRUE).
        auto pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = static_cast<MainWindow*>(pcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        if (!pThis) return FALSE;
        pThis->hwnd_ = hwnd;
        return TRUE;
    }

    if (!pThis) {
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    // Forward messages to instance handlers
    switch (uMsg) {
        case WM_CREATE:         return pThis->OnCreate(reinterpret_cast<LPCREATESTRUCT>(lParam));
        case WM_DESTROY:        return pThis->OnDestroy();
        case WM_SIZE:           return pThis->OnSize(LoWord(wParam), LoWord(lParam), HiWord(lParam));
        case WM_DPICHANGED:     return pThis->OnDpiChanged(wParam, lParam);
        case WM_SETTINGCHANGE:  return pThis->OnSettingChange(LoWord(wParam), reinterpret_cast<LPCTSTR>(lParam));
        case WM_COMMAND:        return pThis->OnCommand(HiWord(wParam), LoWord(wParam), reinterpret_cast<HWND>(lParam));
        case WM_NOTIFY:         return pThis->OnNotify(reinterpret_cast<HWND>(lParam), lParam);
        case WM_ERASEBKGND:     return pThis->OnEraseBkgnd(reinterpret_cast<HDC>(wParam));
        case WM_MOUSEMOVE:      return pThis->OnMouseMove(wParam, lParam);
        case WM_LBUTTONDOWN:    return pThis->OnLButtonDown(wParam, lParam);
        case WM_LBUTTONUP:      return pThis->OnLButtonUp(wParam, lParam);
        case WM_LBUTTONDBLCLK:  return pThis->OnLButtonDblClk(wParam, lParam);
        case WM_PAINT:          return pThis->OnPaint();
        case WM_KEYDOWN:        return pThis->OnKeyDown(wParam, lParam);
        case WM_SYSKEYDOWN:     return pThis->OnSysKeyDown(wParam, lParam);
        case WM_GETMINMAXINFO:  return pThis->OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lParam));
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT MainWindow::OnCreate(LPCREATESTRUCT pcs) {
    // Store pointer again (already done in WM_NCCREATE path, but be safe)
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Apply theme to this window before children are created
    CurrentTheme().Refresh(hwnd_);
    ApplyThemeToWindow(hwnd_);

    InitChildren();

    // Initial layout from WM_SIZE will happen shortly; we still call it here
    // to position children before first show.
    RECT r;
    GetClientRect(hwnd_, &r);
    DoLayout(r.right - r.left, r.bottom - r.top);

    return 0;
}

LRESULT MainWindow::OnDestroy() {
    // Save geometry before destruction
    SaveGeometry();

    // Post quit
    ::PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnSize(UINT wParam, int cx, int cy) {
    // wParam: SIZE_MAXIMIZED, SIZE_MINIMIZED, SIZE_RESTORED, etc.
    DoLayout(cx, cy);
    return 0;
}

LRESULT MainWindow::OnDpiChanged(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    // Manifest already set PerMonitorV2; we recompute layout at the new DPI.
    RECT r;
    GetClientRect(hwnd_, &r);
    DoLayout(r.right - r.left, r.bottom - r.top);
    return 0;
}

LRESULT MainWindow::OnSettingChange(UINT uFlags, LPCTSTR lpszSection) {
    if (lpszSection && (wcscmp(lpszSection, L"ImmersiveColorSet") == 0 ||
                        wcscmp(lpszSection, L"Colors") == 0)) {
        CurrentTheme().Refresh(hwnd_);
        ApplyThemeToWindow(hwnd_);
        // Repaint
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
    return 0;
}

LRESULT MainWindow::OnCommand(WORD wNotifyCode, WORD wID, HWND hwndCtl) {
    // Button clicks and menu commands
    switch (wID) {
        case ID_APP_SCAN:
            StartScan();
            return 0;

        case ID_APP_INSTALL:
            InstallSelectedRequested();
            return 0;

        case ID_APP_UNDO:
            UndoRequested();
            return 0;

        case ID_APP_SETTINGS:
            OpenSettings();
            return 0;

        case ID_APP_THEME_TOGGLE:
            ToggleTheme();
            return 0;

        case ID_APP_REFRESH:
            RefreshRequested();
            return 0;

        default:
            break;
    }

    // Forward to child controls if they handle commands
    if (hwndCtl) {
        return DefWindowProcW(hwnd_, WM_COMMAND,
                              MAKEWPARAM(wID, wNotifyCode),
                              reinterpret_cast<LPARAM>(hwndCtl));
    }
    return 0;
}

LRESULT MainWindow::OnNotify(HWND hwndCtl, LPARAM lParam) {
    if (!hwndCtl) return 0;

    NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
    if (!pnm) return 0;

    if (pnm->code == NM_CUSTOMDRAW) {
        OnDeviceListCustomDraw(pnm);
        return TRUE;
    }

    // Forward to device list
    if (hwndCtl == hDeviceList_) {
        return DeviceList::HandleNotify(hDeviceList_, pnm);
    }

    return 0;
}

LRESULT MainWindow::OnEraseBkgnd(HDC hdc) {
    // Paint the surface background ourselves so it stays consistent with the
    // theme even when Mica is active (Mica fills first; we paint over it if
    // we chose solid).
    if (!Backdrop::MicaAvailable()) {
        return Backdrop::PaintSolidBackground(hwnd_, hdc) ? 1 : 0;
    }
    // With Mica, DWM handles the backdrop; we still need to return a non-zero
    // value to avoid default background erasing, but let Mica show through.
    return 1;
}

LRESULT MainWindow::OnMouseMove(UINT wParam, LPARAM lParam) {
    // Used for splitter drag detection (optional; could be done via tracks).
    return 0;
}

LRESULT MainWindow::OnLButtonDown(UINT wParam, LPARAM lParam) {
    int x = LoWord(lParam);
    int y = HiWord(lParam);

    // Check if click is on the splitter
    if (hSplitter_ && x >= splitterPos_ - 4 && x <= splitterPos_ + 4) {
        SetCapture(hwnd_);
        // Store drag start
        return 0;
    }
    return 0;
}

LRESULT MainWindow::OnLButtonUp(UINT wParam, LPARAM lParam) {
    if (GetCapture() == hwnd_) {
        ReleaseCapture();
        // Adjust splitter position based on mouse move? We track via WM_MOUSEMOVE.
        // For simplicity, we compute from last mouse position.
    }
    return 0;
}

LRESULT MainWindow::OnLButtonDblClk(UINT wParam, LPARAM lParam) {
    // Double-click on device list selects details? Handled in DeviceList.
    return 0;
}

LRESULT MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    if (hdc) {
        // Draw any custom chrome if needed. For now, let children paint.
        EndPaint(hwnd_, &ps);
    }
    return 0;
}

LRESULT MainWindow::OnKeyDown(WPARAM wParam, LPARAM lParam) {
    // Keyboard shortcuts
    if (wParam == VK_F5) {
        RefreshRequested();
        return 0;
    }
    if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        // Select all in device list
        if (hDeviceList_) {
            DeviceList::SelectAll(hDeviceList_);
        }
        return 0;
    }
    if (wParam == VK_DELETE) {
        // Clear selection in device list
        if (hDeviceList_) {
            DeviceList::ClearSelection(hDeviceList_);
        }
        return 0;
    }
    return 0;
}

LRESULT MainWindow::OnSysKeyDown(WPARAM wParam, LPARAM lParam) {
    // Alt+key handling for mnemonics (not used in v1 beyond window menu).
    return 0;
}

LRESULT MainWindow::OnGetMinMaxInfo(LPMINMAXINFO pmmi) {
    pmmi->ptMinTrackSize.x = 980;
    pmmi->ptMinTrackSize.y = 620;
    return 0;
}

void MainWindow::InitChildren() {
    // ----- Header bar -----
    // Create a header container (static control with custom draw or just numeric
    // positioning using a plain panel). For simplicity we use absolute child
    // windows positioned in DoLayout.

    // Search edit box
    hSearchEdit_ = CreateWindowExW(0, L"EDIT", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
                                   ES_AUTOHSCROLL,
                                   0, 0, 200, headerHeight_ - 8,
                                   hwnd_, reinterpret_cast<HMENU>(IDC_SEARCH),
                                   GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hSearchEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(
                     GetStockObject(DEFAULT_GUI_FONT)), MAKELPARAM(FALSE, 0));
    SetWindowTextW(hSearchEdit_, Strings::SearchPlaceholder.data());
    SendMessageW(hSearchEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(6, 6));

    // Theme toggle button (caption toggles between system/light/dark icons)
    hThemeToggle_ = CreateWindowExW(0, L"BUTTON", L"",
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                    0, 0, 32, headerHeight_ - 8,
                                    hwnd_, reinterpret_cast<HMENU>(ID_APP_THEME_TOGGLE),
                                    GetModuleHandleW(nullptr), nullptr);

    // Settings button (gear icon via Segoe Fluent Icons glyph or text "Settings")
    hSettingsBtn_ = CreateWindowExW(0, L"BUTTON", L"",
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                    0, 0, 32, headerHeight_ - 8,
                                    hwnd_, reinterpret_cast<HMENU>(ID_APP_SETTINGS),
                                    GetModuleHandleW(nullptr), nullptr);

    // ----- Toolbar -----
    // Scan button
    hScanBtn_ = CreateWindowExW(0, L"BUTTON", Strings::BtnScan.data(),
                                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                0, 0, 80, toolbarHeight_ - 4,
                                hwnd_, reinterpret_cast<HMENU>(ID_APP_SCAN),
                                GetModuleHandleW(nullptr), nullptr);

    // Install selected button
    hInstallBtn_ = CreateWindowExW(0, L"BUTTON", Strings::BtnInstallSelected.data(),
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                   0, 0, 140, toolbarHeight_ - 4,
                                   hwnd_, reinterpret_cast<HMENU>(ID_APP_INSTALL),
                                   GetModuleHandleW(nullptr), nullptr);

    // Undo last batch button
    hUndoBtn_ = CreateWindowExW(0, L"BUTTON", Strings::BtnUndoLastBatch.data(),
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                0, 0, 140, toolbarHeight_ - 4,
                                hwnd_, reinterpret_cast<HMENU>(ID_APP_UNDO),
                                GetModuleHandleW(nullptr), nullptr);

    // ----- Body split: device list + splitter + details pane -----
    hDeviceList_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT |
                                   LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SORTASCENDING,
                                   0, 0, 400, 200,
                                   hwnd_, reinterpret_cast<HMENU>(IDC_DEVICE_LIST),
                                   GetModuleHandleW(nullptr), nullptr);

    // Splitter (dummy window for hit testing)
    hSplitter_ = CreateWindowExW(0, L"STATIC", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_EX_CLIENTEDGE,
                                 0, 0, 4, 200,
                                 hwnd_, reinterpret_cast<HMENU>(IDC_SPLITTER),
                                 GetModuleHandleW(nullptr), nullptr);

    // Details pane
    hDetailsPane_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", nullptr,
                                    WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                                    0, 0, 300, 200,
                                    hwnd_, reinterpret_cast<HMENU>(IDC_DETAILS),
                                    GetModuleHandleW(nullptr), nullptr);

    // Status bar
    hStatusBar_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                                  WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                  0, 0, 100, statusHeight_,
                                  hwnd_, reinterpret_cast<HMENU>(IDC_STATUS_BAR),
                                  GetModuleHandleW(nullptr), nullptr);

    // Initialize device list
    DeviceList::Init(hDeviceList_);
    DeviceList::SetMainWindow(hwnd_);

    // Initialize details pane
    DetailsPane::Init(hDetailsPane_, hwnd_);

    // Enable visual styles for the controls we created
    // (Common Controls 6 is already enabled via manifest, but ensure it)
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Apply theme to children
    ApplyThemeToWindow(hSearchEdit_);
    ApplyThemeToWindow(hThemeToggle_);
    ApplyThemeToWindow(hSettingsBtn_);
    ApplyThemeToWindow(hScanBtn_);
    ApplyThemeToWindow(hInstallBtn_);
    ApplyThemeToWindow(hUndoBtn_);
    ApplyThemeToWindow(hDeviceList_);
    ApplyThemeToWindow(hDetailsPane_);
    ApplyThemeToWindow(hStatusBar_);
}

void MainWindow::DoLayout(int cx, int cy) {
    if (cx <= 0 || cy <= 0) return;

    int y = 0;

    // Header bar
    int headerH = headerHeight_;
    int toolbarH = toolbarHeight_;
    int statusH = statusHeight_;

    // Spacing
    int gap = 4;

    // Header children
    int headerLeft = 8;
    int headerRight = 8;

    int searchWidth = std::max(120, cx - 160);
    SetWindowPos(hSearchEdit_, nullptr, headerLeft, 4, searchWidth, headerH - 12, SWP_NOZORDER);
    SetWindowPos(hThemeToggle_, nullptr, cx - headerRight - 32 - 32 - 8, 4, 32, headerH - 12, SWP_NOZORDER);
    SetWindowPos(hSettingsBtn_, nullptr, cx - headerRight - 32 - 4, 4, 32, headerH - 12, SWP_NOZORDER);

    y += headerH;

    // Toolbar
    int toolbarTop = y + 2;
    int btnGap = 4;
    int scanW = 80, installW = 140, undoW = 140;
    int leftSpace = 8;
    SetWindowPos(hScanBtn_, nullptr, leftSpace, toolbarTop, scanW, toolbarH - 4, SWP_NOZORDER);
    SetWindowPos(hInstallBtn_, nullptr, leftSpace + scanW + btnGap, toolbarTop, installW, toolbarH - 4, SWP_NOZORDER);
    SetWindowPos(hUndoBtn_, nullptr, leftSpace + scanW + btnGap + installW + btnGap, toolbarTop, undoW, toolbarH - 4, SWP_NOZORDER);

    y += toolbarH + gap;

    // Body area
    int bodyTop = y;
    int bodyBottom = cy - statusH - gap;
    int bodyHeight = bodyBottom - bodyTop;

    // Splitter position: use saved position if valid, else default to 62%
    if (splitterPos_ <= 0) {
        splitterPos_ = static_cast<int>(cx * 0.38); // details pane width (38%)
        splitterPos_ = std::max(splitterMinRight_, std::min(cx - splitterMinLeft_, splitterPos_));
    }

    // Device list (left)
    int listLeft = 0;
    int listWidth = splitterPos_ - 2; // leave 2px for splitter visual
    SetWindowPos(hDeviceList_, nullptr, listLeft, bodyTop, listWidth, bodyHeight, SWP_NOZORDER);

    // Splitter (draggable bar)
    SetWindowPos(hSplitter_, nullptr, splitterPos_ - 2, bodyTop, 4, bodyHeight, SWP_NOZORDER);

    // Details pane (right)
    int detailLeft = splitterPos_;
    int detailWidth = cx - detailLeft;
    SetWindowPos(hDetailsPane_, nullptr, detailLeft, bodyTop, detailWidth, bodyHeight, SWP_NOZORDER);

    // Status bar
    SetWindowPos(hStatusBar_, nullptr, 0, cy - statusH, cx, statusH, SWP_NOZORDER);

    // Update status bar pane count (simple: one pane spanning the width)
    if (hStatusBar_) {
        int partWidth = cx;
        SendMessageW(hStatusBar_, SB_SETPARTS, 1,
                     reinterpret_cast<LPARAM>(&partWidth));
    }
}

void MainWindow::SaveGeometry() {
    if (!hwnd_) return;
    RECT r;
    GetWindowRect(hwnd_, &r);
    App::Settings s = App::LoadSettings();
    s.mainLeft = r.left;
    s.mainTop = r.top;
    s.mainWidth = r.right - r.left;
    s.mainHeight = r.bottom - r.top;
    s.mainMaximized = (GetWindowLongPtrW(hwnd_, GWL_STYLE) & WS_MAXIMIZE) != 0;
    App::SaveSettings(s);
}

void MainWindow::SetDeviceList(std::vector<Hw::Device> devices) {
    if (hDeviceList_) {
        DeviceList::SetDevices(hDeviceList_, std::move(devices));
    }
}

void MainWindow::SetStatusText(std::wstring_view text) {
    if (hStatusBar_) {
        SendMessageW(hStatusBar_, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(
                         const_cast<wchar_t*>(text.data())));
    }
}

void MainWindow::SetTaskText(std::wstring_view text) {
    // For simplicity, append to status or replace. Here we just replace pane 0.
    SetStatusText(text);
}

void MainWindow::EnableInstallButton(bool enabled) {
    if (hInstallBtn_) {
        EnableWindow(hInstallBtn_, enabled);
    }
}

void MainWindow::EnableUndoButton(bool enabled) {
    if (hUndoBtn_) {
        EnableWindow(hUndoBtn_, enabled);
    }
}

void MainWindow::UpdateInstallButtonCount(int count) {
    if (hInstallBtn_) {
        std::wstring text(Strings::BtnInstallSelected);
        if (count > 0) {
            text += L" (" + std::to_wstring(count) + L")";
        }
        SetWindowTextW(hInstallBtn_, text.data());
        EnableWindow(hInstallBtn_, count > 0);
    }
}

void MainWindow::SetScanning(bool scanning) {
    if (hScanBtn_) {
        EnableWindow(hScanBtn_, !scanning);
        SetWindowTextW(hScanBtn_, scanning ?
                       Strings::StatusScanning.data() : Strings::BtnScan.data());
    }
}

void MainWindow::ToggleTheme() {
    auto& app = App::RunningApp();
    auto mode = app.settings.theme;

    if (mode == L"System") {
        // Switch to manual dark
        mode = L"Dark";
        manualDark_ = true;
    } else if (mode == L"Dark") {
        mode = L"Light";
    } else { // Light
        mode = L"System";
        manualDark_ = false;
    }

    app.settings.theme = mode;
    App::SaveSettings(app.settings);
    CurrentTheme().Refresh(hwnd_);
    ApplyThemeToWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::OpenSettings() {
    ShowSettingsDialog(hwnd_);
}

void MainWindow::RefreshRequested() {
    StartScan();
}

void MainWindow::InstallSelectedRequested() {
    if (!hDeviceList_) return;

    // Build (display name, version) pairs for the confirmation dialog from the
    // currently checked driver rows.
    std::vector<std::pair<std::wstring, std::wstring>> drivers;
    const auto ids = DeviceList::GetSelectedInstanceIds(hDeviceList_);
    for (const auto& id : ids) {
        auto device = Hw::DeviceEnum::RefreshOne(id);
        if (!device.instanceId.empty()) {
            drivers.emplace_back(device.description,
                                 device.installedVersion.empty() ? std::wstring(L"unknown")
                                                                 : device.installedVersion);
        }
    }
    if (drivers.empty()) return;

    ShowConfirmInstall(hwnd_, drivers);
}

void MainWindow::UndoRequested() {
    ShowUndoDialog(hwnd_);
}

void MainWindow::StartScan() {
    SetScanning(true);
    // Start scan in worker thread; for now stub, will be wired later.
    // In Phase 2 this will call Hw::DeviceEnum::EnumerateAll and populate the list.
    SetStatusText(Strings::StatusScanning);
    // For now just do a quick inline scan (Phase 2 moves to worker).
    try {
        auto devices = Hw::DeviceEnum::EnumerateAll();
        SetDeviceList(std::move(devices));
        SetStatusText(Strings::StatusIdle);
    } catch (const std::exception&) {
        SetStatusText(Strings::ErrScanFailed);
    }
    SetScanning(false);
}

void MainWindow::OnDeviceListCustomDraw(NMHDR* /*pnm*/) {
    // Custom draw for the device list is handled inside DeviceList::HandleNotify;
    // this hook is kept for window-level drawing that may be added later.
}

} // namespace DriverNest::Ui
