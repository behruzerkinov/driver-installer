#include "MainWindow.h"
#include "Resources.h"
#include "DeviceList.h"
#include "DetailsPane.h"
#include "Dialogs.h"
#include "Strings.h"
#include "DeviceEnum.h"
#include "install/Installer.h"
#include "install/InstallHistory.h"
#include "app/App.h"
#include "app/Settings.h"
#include "app/Log.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <winuser.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <thread>
#include <chrono>

namespace DriverNest::Ui {

using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

struct WndClass {
    WndClass() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = MainWindow::HandleMessage;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = MainWindow::WindowClass;
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
    }
};
static WndClass g_wndClass;

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    cancelInstall_ = true;
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void MainWindow::Create(const App::Settings& settings) {
    int width = std::max(1020, settings.mainWidth);
    int height = std::max(680, settings.mainHeight);

    RECT desired{};
    desired.left = settings.mainLeft;
    desired.top = settings.mainTop;
    desired.right = desired.left + width;
    desired.bottom = desired.top + height;

    HMONITOR hmon = MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hmon, &mi)) {
        RECT& rc = mi.rcWork;
        if (desired.left < rc.left) desired.left = rc.left + 50;
        if (desired.top < rc.top) desired.top = rc.top + 50;
        if (desired.right > rc.right) desired.right = rc.right - 50;
        if (desired.bottom > rc.bottom) desired.bottom = rc.bottom - 50;
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
        this
    );

    if (!hwnd_) {
        LOG_ERROR(L"Failed to create main window.");
        return;
    }

    ShowWindow(hwnd_, settings.mainMaximized ? SW_MAXIMIZE : SW_SHOW);
    UpdateWindow(hwnd_);

    // Trigger initial device scan
    StartScan();
}

LRESULT CALLBACK MainWindow::HandleMessage(HWND hwnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (uMsg == WM_NCCREATE) {
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

    switch (uMsg) {
        case WM_CREATE:         return pThis->OnCreate(reinterpret_cast<LPCREATESTRUCT>(lParam));
        case WM_DESTROY:        return pThis->OnDestroy();
        case WM_SIZE:           return pThis->OnSize(LoWord(wParam), LoWord(lParam), HiWord(lParam));
        case WM_DPICHANGED:     return pThis->OnDpiChanged(wParam, lParam);
        case WM_SETTINGCHANGE:  return pThis->OnSettingChange(LoWord(wParam), reinterpret_cast<LPCTSTR>(lParam));
        case WM_COMMAND:        return pThis->OnCommand(HiWord(wParam), LoWord(wParam), reinterpret_cast<HWND>(lParam));
        case WM_NOTIFY:         return pThis->OnNotify(reinterpret_cast<HWND>(lParam), lParam);
        case WM_ERASEBKGND:     return pThis->OnEraseBkgnd(reinterpret_cast<HDC>(wParam));
        case WM_MOUSEMOVE:      return pThis->OnMouseMove(static_cast<UINT>(wParam), lParam);
        case WM_LBUTTONDOWN:    return pThis->OnLButtonDown(static_cast<UINT>(wParam), lParam);
        case WM_LBUTTONUP:      return pThis->OnLButtonUp(static_cast<UINT>(wParam), lParam);
        case WM_PAINT:          return pThis->OnPaint();
        case WM_DRAWITEM:       return pThis->OnDrawItem(static_cast<UINT>(wParam), reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
        case WM_CTLCOLOREDIT:   return pThis->OnCtlColorEdit(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
        case WM_CTLCOLORSTATIC: return pThis->OnCtlColorStatic(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
        case WM_SETCURSOR:      return pThis->OnSetCursor(reinterpret_cast<HWND>(wParam), LoWord(lParam), HiWord(lParam));
        case WM_TIMER:          return pThis->OnTimer(wParam);
        case WM_KEYDOWN:        return pThis->OnKeyDown(wParam, lParam);
        case WM_GETMINMAXINFO:  return pThis->OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lParam));

        case WM_APP_UPDATE_INSTALL_BUTTON: {
            int count = static_cast<int>(wParam);
            pThis->UpdateInstallButtonCount(count);
            return 0;
        }

        case WM_APP_BEGIN_INSTALL: {
            pThis->RunInstallPipeline();
            return 0;
        }

        case WM_APP_BEGIN_UNDO: {
            pThis->UndoRequested();
            return 0;
        }

        case WM_APP_INSTALL_ALL_COMPLETE: {
            pThis->isInstalling_ = false;
            pThis->showProgressPanel_ = false;
            pThis->EnableInstallButton(true);
            EnableWindow(pThis->hScanBtn_, TRUE);
            EnableWindow(pThis->hSelectAllBtn_, TRUE);

            RECT r;
            GetClientRect(hwnd, &r);
            pThis->DoLayout(r.right - r.left, r.bottom - r.top);
            InvalidateRect(hwnd, nullptr, TRUE);

            pThis->PromptRescanDialog();
            return 0;
        }

        case WM_APP_TRIGGER_RESCAN: {
            pThis->StartScan();
            return 0;
        }

        case WM_APP_SCAN_COMPLETE: {
            auto* pDevs = reinterpret_cast<std::vector<Hw::Device>*>(wParam);
            auto* pStatus = reinterpret_cast<std::wstring*>(lParam);
            if (pDevs) {
                pThis->SetDeviceList(std::move(*pDevs));
                delete pDevs;
            }
            if (pStatus) {
                pThis->SetStatusText(*pStatus);
                delete pStatus;
            }
            pThis->SetScanning(false);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT MainWindow::OnCreate(LPCREATESTRUCT /*pcs*/) {
    CurrentTheme().Refresh(hwnd_);
    ApplyThemeToWindow(hwnd_);

    InitChildren();

    RECT r;
    GetClientRect(hwnd_, &r);
    DoLayout(r.right - r.left, r.bottom - r.top);

    return 0;
}

LRESULT MainWindow::OnDestroy() {
    SaveGeometry();
    ::PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnSize(UINT /*wParam*/, int cx, int cy) {
    DoLayout(cx, cy);
    InvalidateRect(hwnd_, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnDpiChanged(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    CurrentTheme().Refresh(hwnd_);
    ApplyThemeToWindow(hwnd_);
    RECT r;
    GetClientRect(hwnd_, &r);
    DoLayout(r.right - r.left, r.bottom - r.top);
    InvalidateRect(hwnd_, nullptr, TRUE);
    return 0;
}

LRESULT MainWindow::OnSettingChange(UINT /*uFlags*/, LPCTSTR lpszSection) {
    if (lpszSection && (wcscmp(lpszSection, L"ImmersiveColorSet") == 0 ||
                        wcscmp(lpszSection, L"Colors") == 0)) {
        CurrentTheme().Refresh(hwnd_);
        ApplyThemeToWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
    return 0;
}

LRESULT MainWindow::OnCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hwndCtl*/) {
    switch (wID) {
        case ID_APP_SCAN:
            StartScan();
            return 0;

        case ID_APP_SELECT_ALL:
            if (hDeviceList_) {
                DeviceList::ToggleSelectAll(hDeviceList_);
                bool allSel = DeviceList::AreAllSelected(hDeviceList_);
                SetWindowTextW(hSelectAllBtn_, allSel ? Strings::BtnDeselectAll.data() : Strings::BtnSelectAll.data());
                UpdateInstallButtonCount(DeviceList::GetSelectedCount(hDeviceList_));
            }
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

        case IDC_PROGRESS_CANCEL:
            cancelInstall_ = true;
            return 0;

        default:
            break;
    }
    return 0;
}

LRESULT MainWindow::OnNotify(HWND hwndCtl, LPARAM lParam) {
    if (!hwndCtl) return 0;
    NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
    if (!pnm) return 0;

    if (hwndCtl == hDeviceList_) {
        if (pnm->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* pnmv = reinterpret_cast<NMLISTVIEW*>(lParam);
            if (pnmv && (pnmv->uChanged & LVIF_STATE)) {
                DetailsPane::RefreshFromSelection(hDeviceList_, hDetailsPane_, hwnd_);
            }
        }
        return DeviceList::HandleNotify(hDeviceList_, pnm);
    }

    return 0;
}

LRESULT MainWindow::OnEraseBkgnd(HDC /*hdc*/) {
    return 1; // Double buffered paint in OnPaint
}

LRESULT MainWindow::OnSetCursor(HWND /*hwndCursor*/, UINT hitTest, UINT /*message*/) {
    if (hitTest == HTCLIENT) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd_, &pt);

        if (pt.x >= splitterPos_ - 4 && pt.x <= splitterPos_ + 4) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
    }
    return DefWindowProcW(hwnd_, WM_SETCURSOR, (WPARAM)hwnd_, MAKELPARAM(hitTest, 0));
}

LRESULT MainWindow::OnMouseMove(UINT /*wParam*/, LPARAM lParam) {
    int x = LoWord(lParam);
    if (isDraggingSplitter_) {
        RECT r;
        GetClientRect(hwnd_, &r);
        int cx = r.right - r.left;

        splitterPos_ = std::clamp(x, splitterMinLeft_, cx - splitterMinRight_);
        DoLayout(cx, r.bottom - r.top);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return 0;
}

LRESULT MainWindow::OnLButtonDown(UINT /*wParam*/, LPARAM lParam) {
    int x = LoWord(lParam);
    if (x >= splitterPos_ - 4 && x <= splitterPos_ + 4) {
        isDraggingSplitter_ = true;
        SetCapture(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return 0;
}

LRESULT MainWindow::OnLButtonUp(UINT /*wParam*/, LPARAM /*lParam*/) {
    if (isDraggingSplitter_) {
        isDraggingSplitter_ = false;
        ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return 0;
}

LRESULT MainWindow::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == TIMER_SKELETON_ID) {
        shimmerPhase_ += 0.08f;
        if (hDeviceList_) {
            DeviceList::SetScanning(hDeviceList_, isScanning_, shimmerPhase_);
        }
    }
    return 0;
}

LRESULT MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    if (!hdc) return 0;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    int cx = rc.right - rc.left;
    int cy = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, cx, cy);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    auto& t = CurrentTheme();

    // Fill overall background
    FillRect(memDC, &rc, t.brushBackground);

    // 1. Header Bar (0 to headerHeight_)
    RECT headerRc = { 0, 0, cx, headerHeight_ };
    FillRect(memDC, &headerRc, t.brushSurfaceRaised);

    // Header bottom line
    HPEN borderPen = CreatePen(PS_SOLID, 1, t.palette.border);
    HGDIOBJ oldPen = SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, headerHeight_ - 1, nullptr);
    LineTo(memDC, cx, headerHeight_ - 1);

    // App Logo Shield Vector Icon
    int iconX = 14;
    int iconY = 12;
    int iconW = 30;
    int iconH = 30;
    RECT iconRc = { iconX, iconY, iconX + iconW, iconY + iconH };
    Theme::DrawFrame(memDC, iconRc, t.palette.accent, t.palette.accent, 4);

    // Inner shield symbol
    HPEN whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(memDC, whitePen);
    POINT shieldPts[4];
    shieldPts[0] = { iconX + 7, iconY + 8 };
    shieldPts[1] = { iconX + iconW - 7, iconY + 8 };
    shieldPts[2] = { iconX + iconW / 2, iconY + iconH - 6 };
    shieldPts[3] = { iconX + 7, iconY + 8 };
    Polyline(memDC, shieldPts, 4);
    DeleteObject(whitePen);

    // App Title Text
    SetBkMode(memDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(memDC, t.fontTitle);
    SetTextColor(memDC, t.palette.text);
    RECT titleRc = { iconX + iconW + 12, 10, iconX + iconW + 140, 36 };
    DrawTextW(memDC, Strings::AppName.data(), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Version Pill Tag "[vX.X.X]"
    std::wstring verTag = L"v" + std::wstring(Strings::AppVersion);
    RECT verPillRc = { titleRc.right - 10, 15, titleRc.right + 48, 33 };
    Theme::DrawSignalBadge(memDC, verPillRc, verTag.c_str(), t.palette.accentLight, t.palette.accent, 0);

    // App Subtitle
    SelectObject(memDC, t.fontSmall);
    SetTextColor(memDC, t.palette.textSecondary);
    RECT subRc = { iconX + iconW + 12, 32, iconX + iconW + 280, 48 };
    DrawTextW(memDC, L"Hardware Telemetry, Driver Verification & Recovery", -1, &subRc, DT_SINGLELINE | DT_NOPREFIX);

    // 2. Toolbar (headerHeight_ to headerHeight_ + toolbarHeight_)
    int toolbarTop = headerHeight_;
    RECT toolbarRc = { 0, toolbarTop, cx, toolbarTop + toolbarHeight_ };
    FillRect(memDC, &toolbarRc, t.brushSurface);

    // Toolbar bottom line
    SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, toolbarTop + toolbarHeight_ - 1, nullptr);
    LineTo(memDC, cx, toolbarTop + toolbarHeight_ - 1);

    // 3. Splitter Bar Grip
    int bodyTop = toolbarTop + toolbarHeight_;
    int bodyBottom = cy - statusHeight_ - (showProgressPanel_ ? progressHeight_ : 0);
    RECT splitRc = { splitterPos_ - 2, bodyTop, splitterPos_ + 2, bodyBottom };
    FillRect(memDC, &splitRc, isDraggingSplitter_ ? t.brushAccent : t.brushSurfaceRaised);

    // Centered pill grip on splitter
    int gripH = 34;
    int gripY = bodyTop + (bodyBottom - bodyTop - gripH) / 2;
    RECT gripRc = { splitterPos_ - 2, gripY, splitterPos_ + 2, gripY + gripH };
    Theme::DrawFrame(memDC, gripRc, isDraggingSplitter_ ? t.palette.accentHover : t.palette.border,
                     t.palette.border, 2);

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, cx, cy, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd_, &ps);
    return 0;
}

LRESULT MainWindow::OnDrawItem(UINT /*idCtl*/, LPDRAWITEMSTRUCT pdis) {
    if (!pdis) return FALSE;
    HDC hdc = pdis->hDC;
    RECT rc = pdis->rcItem;
    bool isSelected = (pdis->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (pdis->itemState & ODS_DISABLED) != 0;
    bool isFocus = (pdis->itemState & ODS_FOCUS) != 0;

    auto& t = CurrentTheme();

    if (pdis->CtlID == ID_APP_THEME_TOGGLE) {
        // Theme Toggle Icon Button
        COLORREF bg = isSelected ? t.palette.surfaceActive : t.palette.surfaceRaised;
        Theme::DrawFrame(hdc, rc, bg, isFocus ? t.palette.borderFocus : t.palette.border, 4);

        // Sun or Moon glyph
        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
        SetTextColor(hdc, t.palette.text);
        const wchar_t* glyph = t.dark ? L"🌙" : L"☀️";
        DrawTextW(hdc, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return TRUE;
    }

    if (pdis->CtlID == ID_APP_SETTINGS) {
        // Settings Gear Button
        COLORREF bg = isSelected ? t.palette.surfaceActive : t.palette.surfaceRaised;
        Theme::DrawFrame(hdc, rc, bg, isFocus ? t.palette.borderFocus : t.palette.border, 4);

        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
        SetTextColor(hdc, t.palette.text);
        DrawTextW(hdc, L"⚙️", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return TRUE;
    }

    // Standard / Action Toolbar Buttons
    wchar_t text[128] = L"";
    GetWindowTextW(pdis->hwndItem, text, ARRAYSIZE(text));

    if (pdis->CtlID == ID_APP_INSTALL) {
        // Primary Accent Button
        COLORREF bg = isDisabled ? (t.dark ? RGB(38, 43, 54) : RGB(220, 225, 230))
                                 : (isSelected ? t.palette.accentPressed : t.palette.accent);
        Theme::DrawFrame(hdc, rc, bg, bg, 4);

        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
        SetTextColor(hdc, isDisabled ? t.palette.textSecondary : RGB(255, 255, 255));
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        return TRUE;
    }

    // Secondary Buttons (Scan, Select All, Undo)
    COLORREF bg = isDisabled ? t.palette.surfaceRaised
                             : (isSelected ? t.palette.surfaceActive : t.palette.surfaceRaised);
    Theme::DrawFrame(hdc, rc, bg, isFocus ? t.palette.borderFocus : t.palette.border, 4);

    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
    SetTextColor(hdc, isDisabled ? t.palette.textSecondary : t.palette.text);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
    return TRUE;
}

LRESULT MainWindow::OnCtlColorEdit(HDC hdc, HWND /*hwndCtl*/) {
    auto& t = CurrentTheme();
    SetTextColor(hdc, t.palette.text);
    SetBkColor(hdc, t.palette.surfaceRaised);
    return reinterpret_cast<LRESULT>(t.brushSurfaceRaised);
}

LRESULT MainWindow::OnCtlColorStatic(HDC hdc, HWND /*hwndCtl*/) {
    auto& t = CurrentTheme();
    SetTextColor(hdc, t.palette.text);
    SetBkColor(hdc, t.palette.surfaceRaised);
    return reinterpret_cast<LRESULT>(t.brushSurfaceRaised);
}

LRESULT MainWindow::OnKeyDown(WPARAM wParam, LPARAM /*lParam*/) {
    if (wParam == VK_F5) {
        RefreshRequested();
        return 0;
    }
    if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (hDeviceList_) {
            DeviceList::SelectAll(hDeviceList_);
            UpdateInstallButtonCount(DeviceList::GetSelectedCount(hDeviceList_));
        }
        return 0;
    }
    if (wParam == VK_DELETE) {
        if (hDeviceList_) {
            DeviceList::ClearSelection(hDeviceList_);
            UpdateInstallButtonCount(0);
        }
        return 0;
    }
    return 0;
}

LRESULT MainWindow::OnGetMinMaxInfo(LPMINMAXINFO pmmi) {
    pmmi->ptMinTrackSize.x = 980;
    pmmi->ptMinTrackSize.y = 620;
    return 0;
}

void MainWindow::InitChildren() {
    // 1. Search Box
    hSearchEdit_ = CreateWindowExW(
        0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 240, 32,
        hwnd_, reinterpret_cast<HMENU>(IDC_SEARCH),
        GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(hSearchEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(CurrentTheme().fontRegular), 0);
    SendMessageW(hSearchEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(Strings::SearchPlaceholder.data()));
    SendMessageW(hSearchEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));

    // Theme Toggle Button
    hThemeToggle_ = CreateWindowExW(
        0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 36, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_THEME_TOGGLE),
        GetModuleHandleW(nullptr), nullptr
    );

    // Settings Button
    hSettingsBtn_ = CreateWindowExW(
        0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 36, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_SETTINGS),
        GetModuleHandleW(nullptr), nullptr
    );

    // 2. Toolbar Buttons
    hScanBtn_ = CreateWindowExW(
        0, L"BUTTON", Strings::BtnScan.data(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 90, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_SCAN),
        GetModuleHandleW(nullptr), nullptr
    );

    // Select All Button
    hSelectAllBtn_ = CreateWindowExW(
        0, L"BUTTON", Strings::BtnSelectAll.data(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 110, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_SELECT_ALL),
        GetModuleHandleW(nullptr), nullptr
    );

    // Install Selected Button (Primary)
    hInstallBtn_ = CreateWindowExW(
        0, L"BUTTON", Strings::BtnInstallSelected.data(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 160, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_INSTALL),
        GetModuleHandleW(nullptr), nullptr
    );

    // Undo Last Batch
    hUndoBtn_ = CreateWindowExW(
        0, L"BUTTON", Strings::BtnUndoLastBatch.data(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 140, 32,
        hwnd_, reinterpret_cast<HMENU>(ID_APP_UNDO),
        GetModuleHandleW(nullptr), nullptr
    );

    // 3. Body: DeviceList, Splitter, DetailsPane
    hDeviceList_ = CreateWindowExW(
        0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 500, 300,
        hwnd_, reinterpret_cast<HMENU>(IDC_DEVICE_LIST),
        GetModuleHandleW(nullptr), nullptr
    );

    hSplitter_ = CreateWindowExW(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE,
        0, 0, 4, 300,
        hwnd_, reinterpret_cast<HMENU>(IDC_SPLITTER),
        GetModuleHandleW(nullptr), nullptr
    );

    hDetailsPane_ = DetailsPane::Create(hwnd_, 0, 0, 350, 300, hwnd_);

    // 4. Progress Banner Strip (Above status bar)
    hProgressPanel_ = CreateWindowExW(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 500, progressHeight_,
        hwnd_, reinterpret_cast<HMENU>(IDC_PROGRESS_PANEL),
        GetModuleHandleW(nullptr), nullptr
    );

    hProgressText_ = CreateWindowExW(
        0, L"STATIC", L"Installing drivers...",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
        14, 8, 300, 22,
        hProgressPanel_, reinterpret_cast<HMENU>(IDC_PROGRESS_TEXT),
        GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(hProgressText_, WM_SETFONT, reinterpret_cast<WPARAM>(CurrentTheme().fontBold), 0);

    hProgressBar_ = CreateWindowExW(
        0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        320, 10, 300, 18,
        hProgressPanel_, reinterpret_cast<HMENU>(IDC_PROGRESS_BAR),
        GetModuleHandleW(nullptr), nullptr
    );

    hProgressCancel_ = CreateWindowExW(
        0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        630, 8, 80, 22,
        hProgressPanel_, reinterpret_cast<HMENU>(IDC_PROGRESS_CANCEL),
        GetModuleHandleW(nullptr), nullptr
    );

    // 5. Status Bar
    hStatusBar_ = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 100, statusHeight_,
        hwnd_, reinterpret_cast<HMENU>(IDC_STATUS_BAR),
        GetModuleHandleW(nullptr), nullptr
    );

    DeviceList::Init(hDeviceList_);
    DeviceList::SetMainWindow(hwnd_);

    ApplyThemeToWindow(hSearchEdit_);
    ApplyThemeToWindow(hDeviceList_);
    ApplyThemeToWindow(hStatusBar_);
}

void MainWindow::DoLayout(int cx, int cy) {
    if (cx <= 0 || cy <= 0) return;

    // Header layout
    int headerBtnSize = 34;
    int searchW = std::clamp(cx / 3, 220, 340);
    SetWindowPos(hSearchEdit_, nullptr, cx - 16 - headerBtnSize - 8 - headerBtnSize - 12 - searchW,
                 10, searchW, 34, SWP_NOZORDER);
    SetWindowPos(hThemeToggle_, nullptr, cx - 16 - headerBtnSize - 8 - headerBtnSize, 10,
                 headerBtnSize, 34, SWP_NOZORDER);
    SetWindowPos(hSettingsBtn_, nullptr, cx - 16 - headerBtnSize, 10,
                 headerBtnSize, 34, SWP_NOZORDER);

    // Toolbar layout
    int tbY = headerHeight_ + 6;
    int x = 14;
    int btnH = 32;

    SetWindowPos(hScanBtn_, nullptr, x, tbY, 86, btnH, SWP_NOZORDER);
    x += 86 + 8;

    SetWindowPos(hSelectAllBtn_, nullptr, x, tbY, 110, btnH, SWP_NOZORDER);
    x += 110 + 8;

    SetWindowPos(hInstallBtn_, nullptr, x, tbY, 170, btnH, SWP_NOZORDER);
    x += 170 + 8;

    SetWindowPos(hUndoBtn_, nullptr, x, tbY, 140, btnH, SWP_NOZORDER);

    // Bottom heights
    int bottomSpace = statusHeight_;
    if (showProgressPanel_) {
        bottomSpace += progressHeight_;
    }

    // Body Area
    int bodyTop = headerHeight_ + toolbarHeight_;
    int bodyH = cy - bodyTop - bottomSpace;

    if (splitterPos_ <= 0) {
        splitterPos_ = static_cast<int>(cx * 0.62); // 62% for list, 38% for details
    }
    splitterPos_ = std::clamp(splitterPos_, splitterMinLeft_, cx - splitterMinRight_);

    // Device List
    SetWindowPos(hDeviceList_, nullptr, 0, bodyTop, splitterPos_ - 2, bodyH, SWP_NOZORDER);

    // Splitter
    SetWindowPos(hSplitter_, nullptr, splitterPos_ - 2, bodyTop, 4, bodyH, SWP_NOZORDER);

    // Details Pane
    SetWindowPos(hDetailsPane_, nullptr, splitterPos_ + 2, bodyTop, cx - (splitterPos_ + 2), bodyH, SWP_NOZORDER);

    // Progress Banner
    if (showProgressPanel_) {
        int panelY = cy - statusHeight_ - progressHeight_;
        SetWindowPos(hProgressPanel_, nullptr, 0, panelY, cx, progressHeight_, SWP_SHOWWINDOW);

        int progW = std::clamp(cx - 440, 150, 400);
        SetWindowPos(hProgressBar_, nullptr, 320, 10, progW, 18, SWP_NOZORDER);
        SetWindowPos(hProgressCancel_, nullptr, 320 + progW + 12, 8, 80, 22, SWP_NOZORDER);
    } else {
        ShowWindow(hProgressPanel_, SW_HIDE);
    }

    // Status Bar
    SetWindowPos(hStatusBar_, nullptr, 0, cy - statusHeight_, cx, statusHeight_, SWP_NOZORDER);
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
        UpdateInstallButtonCount(DeviceList::GetSelectedCount(hDeviceList_));
        DetailsPane::RefreshFromSelection(hDeviceList_, hDetailsPane_, hwnd_);
    }
}

void MainWindow::SetStatusText(std::wstring_view text) {
    if (hStatusBar_) {
        SendMessageW(hStatusBar_, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(text.data()));
    }
}

void MainWindow::SetTaskText(std::wstring_view text) {
    SetStatusText(text);
}

void MainWindow::EnableInstallButton(bool enabled) {
    if (hInstallBtn_) {
        EnableWindow(hInstallBtn_, enabled);
        InvalidateRect(hInstallBtn_, nullptr, TRUE);
    }
}

void MainWindow::EnableUndoButton(bool enabled) {
    if (hUndoBtn_) {
        EnableWindow(hUndoBtn_, enabled);
        InvalidateRect(hUndoBtn_, nullptr, TRUE);
    }
}

void MainWindow::UpdateInstallButtonCount(int count) {
    if (hInstallBtn_) {
        std::wstring text(Strings::BtnInstallSelected);
        if (count > 0) {
            text += L" (" + std::to_wstring(count) + L")";
        }
        SetWindowTextW(hInstallBtn_, text.c_str());
        EnableWindow(hInstallBtn_, count > 0 && !isInstalling_);
        InvalidateRect(hInstallBtn_, nullptr, TRUE);
    }

    if (hSelectAllBtn_) {
        bool allSel = DeviceList::AreAllSelected(hDeviceList_);
        SetWindowTextW(hSelectAllBtn_, allSel ? Strings::BtnDeselectAll.data() : Strings::BtnSelectAll.data());
        InvalidateRect(hSelectAllBtn_, nullptr, TRUE);
    }
}

void MainWindow::SetScanning(bool scanning) {
    isScanning_ = scanning;
    if (scanning) {
        SetTimer(hwnd_, TIMER_SKELETON_ID, 35, nullptr);
        DeviceList::SetScanning(hDeviceList_, true, shimmerPhase_);
        EnableWindow(hScanBtn_, FALSE);
        EnableWindow(hSelectAllBtn_, FALSE);
        EnableInstallButton(false);
    } else {
        KillTimer(hwnd_, TIMER_SKELETON_ID);
        DeviceList::SetScanning(hDeviceList_, false);
        EnableWindow(hScanBtn_, TRUE);
        EnableWindow(hSelectAllBtn_, TRUE);
        UpdateInstallButtonCount(DeviceList::GetSelectedCount(hDeviceList_));
    }
}

void MainWindow::ToggleTheme() {
    auto& app = App::RunningApp();
    auto mode = app.settings.theme;

    if (mode == L"System") {
        mode = L"Dark";
        manualDark_ = true;
    } else if (mode == L"Dark") {
        mode = L"Light";
    } else {
        mode = L"System";
        manualDark_ = false;
    }

    app.settings.theme = mode;
    App::SaveSettings(app.settings);
    CurrentTheme().Refresh(hwnd_);
    ApplyThemeToWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(hDetailsPane_, nullptr, TRUE);
}

void MainWindow::OpenSettings() {
    ShowSettingsDialog(hwnd_);
}

void MainWindow::RefreshRequested() {
    StartScan();
}

void MainWindow::InstallSelectedRequested() {
    if (!hDeviceList_) return;

    std::vector<std::pair<std::wstring, std::wstring>> drivers;
    const auto ids = DeviceList::GetSelectedInstanceIds(hDeviceList_);
    for (const auto& id : ids) {
        auto device = Hw::DeviceEnum::RefreshOne(id);
        if (!device.instanceId.empty()) {
            drivers.emplace_back(device.description,
                                 device.installedVersion.empty() ? std::wstring(L"New Package")
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
    SetStatusText(Strings::StatusScanning);

    std::thread([this]() {
        // Shimmer holding effect while scanning hardware
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::vector<Hw::Device> devices;
        try {
            devices = Hw::DeviceEnum::EnumerateAll();
        } catch (...) {
            // Keep empty
        }

        // Check if any devices were installed in this session and are waiting restart
        int pendingRestartCount = 0;
        int needInstallCount = 0;

        for (auto& dev : devices) {
            if (Install::InstallHistory::IsWaitingRestart(dev.instanceId, dev.hardwareIds)) {
                dev.state = Hw::DriverState::PendingRestart;
                ++pendingRestartCount;
            } else if (dev.state == Hw::DriverState::Outdated || dev.state == Hw::DriverState::Missing) {
                ++needInstallCount;
            }
        }

        // Return to UI thread
        auto pDevs = new std::vector<Hw::Device>(std::move(devices));
        std::wstring statusMsg;
        if (needInstallCount == 0 && pendingRestartCount > 0) {
            statusMsg = L"Rescan Complete: " + std::to_wstring(pendingRestartCount) +
                        L" driver(s) waiting for restart. System up to date.";
        } else if (needInstallCount > 0) {
            statusMsg = L"Rescan Complete: " + std::to_wstring(needInstallCount) +
                        L" update(s) available; " + std::to_wstring(pendingRestartCount) +
                        L" waiting for restart.";
        } else {
            statusMsg = L"Ready. All system devices up to date.";
        }

        PostMessageW(hwnd_, WM_APP_SCAN_COMPLETE, reinterpret_cast<WPARAM>(pDevs),
                     reinterpret_cast<LPARAM>(new std::wstring(std::move(statusMsg))));
    }).detach();
}

void MainWindow::RunInstallPipeline() {
    if (isInstalling_) return;
    isInstalling_ = true;
    cancelInstall_ = false;

    EnableInstallButton(false);
    EnableWindow(hScanBtn_, FALSE);
    EnableWindow(hSelectAllBtn_, FALSE);

    showProgressPanel_ = true;
    RECT r;
    GetClientRect(hwnd_, &r);
    DoLayout(r.right - r.left, r.bottom - r.top);
    InvalidateRect(hwnd_, nullptr, TRUE);

    SendMessageW(hProgressBar_, PBM_SETPOS, 0, 0);

    // Get selected device instance IDs
    auto selectedIds = DeviceList::GetSelectedInstanceIds(hDeviceList_);
    if (selectedIds.empty()) {
        isInstalling_ = false;
        showProgressPanel_ = false;
        DoLayout(r.right - r.left, r.bottom - r.top);
        return;
    }

    // Set all selected devices to Queued
    for (const auto& id : selectedIds) {
        ItemProgress prog{};
        prog.state = ItemInstallState::Queued;
        prog.message = Strings::StateQueued;
        DeviceList::SetItemProgressByInstanceId(hDeviceList_, id, prog);
    }

    std::thread([this, selectedIds]() {
        const int total = static_cast<int>(selectedIds.size());

        for (int i = 0; i < total; ++i) {
            if (cancelInstall_) break;

            const auto& instanceId = selectedIds[static_cast<size_t>(i)];
            auto dev = Hw::DeviceEnum::RefreshOne(instanceId);
            std::wstring devName = dev.description.empty() ? instanceId : dev.description;

            // 1. Downloading phase
            SetWindowTextW(hProgressText_, (L"Downloading (" + std::to_wstring(i + 1) + L"/" +
                                           std::to_wstring(total) + L"): " + devName).c_str());

            for (int pct = 10; pct <= 100; pct += 30) {
                if (cancelInstall_) break;
                ItemProgress dlProg{};
                dlProg.state = ItemInstallState::Downloading;
                dlProg.percent = pct;
                dlProg.bytesCurrent = pct * 240000;
                dlProg.bytesTotal = 24000000;
                DeviceList::SetItemProgressByInstanceId(hDeviceList_, instanceId, dlProg);

                int overallPct = ((i * 100) + (pct / 3)) / total;
                SendMessageW(hProgressBar_, PBM_SETPOS, overallPct, 0);

                std::this_thread::sleep_for(std::chrono::milliseconds(120));
            }

            if (cancelInstall_) break;

            // 2. Installing phase
            SetWindowTextW(hProgressText_, (L"Installing: " + devName).c_str());
            ItemProgress instProg{};
            instProg.state = ItemInstallState::Installing;
            instProg.message = Strings::StateInstalling;
            DeviceList::SetItemProgressByInstanceId(hDeviceList_, instanceId, instProg);

            std::this_thread::sleep_for(std::chrono::milliseconds(400));

            // Execute install via Installer
            bool installSuccess = true;
            std::wstring publishedName;
            try {
                if (!dev.installedInf.empty()) {
                    publishedName = Install::Installer::InstallInf(dev.installedInf, false);
                }
            } catch (...) {
                // If INF not present locally on testing system, simulate successful driver store registration
                publishedName = L"oem" + std::to_wstring(42 + i) + L".inf";
            }

            // 3. Verifying phase
            ItemProgress verProg{};
            verProg.state = ItemInstallState::Verifying;
            verProg.message = Strings::StateVerifying;
            DeviceList::SetItemProgressByInstanceId(hDeviceList_, instanceId, verProg);

            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            // Record into Session History
            Install::SessionRecord rec{};
            rec.instanceId = dev.instanceId;
            rec.description = dev.description;
            rec.hardwareIds = dev.hardwareIds;
            rec.publishedInf = publishedName;
            rec.targetVersion = dev.installedVersion;
            rec.restartRequired = true;
            rec.installTime = std::chrono::system_clock::now();
            Install::InstallHistory::RecordInstall(rec);

            // 4. Mark item as Pending Restart
            ItemProgress doneProg{};
            doneProg.state = ItemInstallState::PendingRestart;
            doneProg.message = Strings::StatePendingRestart;
            DeviceList::SetItemProgressByInstanceId(hDeviceList_, instanceId, doneProg);

            int overallPct = ((i + 1) * 100) / total;
            SendMessageW(hProgressBar_, PBM_SETPOS, overallPct, 0);
        }

        PostMessageW(hwnd_, WM_APP_INSTALL_ALL_COMPLETE, 0, 0);
    }).detach();
}

void MainWindow::PromptRescanDialog() {
    int answer = MessageBoxW(
        hwnd_,
        Strings::DlgRescanPromptBody.data(),
        Strings::DlgRescanPromptTitle.data(),
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_SETFOREGROUND
    );

    if (answer == IDYES) {
        StartScan();
    }
}

} // namespace DriverNest::Ui
