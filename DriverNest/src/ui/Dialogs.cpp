#include "Dialogs.h"
#include "Resources.h"
#include "Theme.h"
#include "Backdrop.h"
#include "Strings.h"
#include "app/App.h"
#include "app/Settings.h"
#include "app/Log.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <vsstyle.h>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <memory>
#include <sstream>
#include <iomanip>
#include <shlobj.h>

namespace DriverNest::Ui {

namespace {

struct SettingsDlgCtx {
    HWND hwndOwner;
};

HRESULT CALLBACK DarkTaskDialogCallback(HWND hwnd, UINT notification, WPARAM, LPARAM, LONG_PTR) {
    if (notification == TDN_CREATED) {
        Theme::SetDarkMode(hwnd, true);
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
    return S_OK;
}

std::wstring BuildDiagnosticsString();

// --- Shared dialog procedure table ---
// Each dialog uses a control ID namespace to avoid collisions.
// We prefix dialog-specific controls with a unique base per dialog.

// Dialog procedure for the Settings dialog.
INT_PTR CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_INITDIALOG) {
        auto* pCtx = reinterpret_cast<struct SettingsDlgCtx*>(lParam);
        SetWindowLongPtrW(hwnd, DWLP_USER, lParam);

        // Load current settings
        auto settings = App::LoadSettings();
        // Populate controls (we need HWNDs; we'll create them dynamically or
        // use a dialog template. For this phase we manually create a window
        // with controls positioned, not a dialog template.)

        // For now, this is a stub showing the pattern. Full dialog templates
        // would go in a .rc file. We'll implement a custom settings window
        // instead to avoid needing .rc dialog templates.
        EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return FALSE;
}

// --- Settings window (custom, not a dialog template) ---
void ShowSettingsWindow(HWND hwndOwner) {
    auto& app = App::RunningApp();
    enum : int { ChoosePackFolder = 1001, CycleAccent = 1002, ToggleUnsigned = 1003 };
    bool rescanRequired = false;
    for (;;) {
        std::wstring content = L"Driver pack folder: ";
        content += app.settings.packRoot.empty() ? L"(none)" : app.settings.packRoot.wstring();
        content += L"\n\nUnsigned candidates: ";
        content += app.settings.showUnsigned ? L"shown" : L"hidden";
        std::wstring accentButton = std::wstring(L"Accent color: ") + app.settings.accent + L" (change)";
        TASKDIALOG_BUTTON buttons[] = {
            { ChoosePackFolder, L"Choose driver pack folder..." },
            { CycleAccent, accentButton.c_str() },
            { ToggleUnsigned, app.settings.showUnsigned ? L"Hide unsigned candidates" : L"Show unsigned candidates" }
        };
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        config.hwndParent = hwndOwner;
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
        config.pszWindowTitle = Strings::DlgSettingsTitle.data();
        config.pszMainInstruction = L"DriverNest settings";
        config.pszContent = content.c_str();
        config.pfCallback = DarkTaskDialogCallback;
        config.cButtons = ARRAYSIZE(buttons);
        config.pButtons = buttons;
        config.nDefaultButton = ChoosePackFolder;
        int pressed = IDCANCEL;
        if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr))) break;
        if (pressed == ChoosePackFolder) {
            BROWSEINFOW browse{};
            browse.hwndOwner = hwndOwner;
            browse.lpszTitle = L"Choose the folder containing your driver packs";
            browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE selected = SHBrowseForFolderW(&browse);
            if (selected) {
                wchar_t folder[MAX_PATH]{};
                if (SHGetPathFromIDListW(selected, folder)) {
                    app.settings.packRoot = folder;
                    App::SaveSettings(app.settings);
                    rescanRequired = true;
                }
                CoTaskMemFree(selected);
            }
        } else if (pressed == CycleAccent) {
            app.settings.accent = Theme::NextAccentName(app.settings.accent);
            PostMessageW(hwndOwner, WM_APP_REFRESH_THEME, 0, 0);
            App::SaveSettings(app.settings);
        } else if (pressed == ToggleUnsigned) {
            app.settings.showUnsigned = !app.settings.showUnsigned;
            App::SaveSettings(app.settings);
            rescanRequired = true;
        } else {
            break;
        }
    }
    if (rescanRequired) PostMessageW(hwndOwner, WM_APP_TRIGGER_RESCAN, 0, 0);
}

// --- About dialog ---
void ShowAboutWindow(HWND hwndOwner) {
    HWND hwnd = CreateWindowExW(
        0,
        L"STATIC",
        Strings::DlgAboutTitle.data(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, 400, 320,
        hwndOwner, nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );
    if (!hwnd) return;

    RECT rOwner;
    GetWindowRect(hwndOwner, &rOwner);
    int w = 400, h = 320;
    SetWindowPos(hwnd, HWND_TOP,
                 rOwner.left + (rOwner.right - rOwner.left - w) / 2,
                 rOwner.top + (rOwner.bottom - rOwner.top - h) / 2,
                 w, h, SWP_NOZORDER);

    CurrentTheme().Refresh(hwnd);
    ApplyThemeToWindow(hwnd);

    // Simple labels
    int y = 16;
    auto AddLabel = [&](int y, std::wstring_view text, bool center = false) {
        DWORD style = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
        if (center) style |= SS_CENTER;
        HWND h = CreateWindowExW(0, L"STATIC", nullptr, style,
                                 16, y, 368, 20, hwnd, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(
                         GetStockObject(DEFAULT_GUI_FONT)), MAKELPARAM(FALSE, 0));
        SetWindowTextW(h, text.data());
        return h;
    };

    AddLabel(y, std::wstring(Strings::DlgAboutVersion) + std::wstring(Strings::AppVersion)); y += 24;
    AddLabel(y, Strings::DlgAboutLicense); y += 24;

    // Project URL (placeholder; filled at build time via CMake var DN_PROJECT_URL)
    // We store it in a string table; for now empty but must not be empty per spec.
    // We'll set it from a compile-time define.
    wchar_t projectUrl[256] = L"";
#ifdef DN_PROJECT_URL
    wcscpy_s(projectUrl, DN_PROJECT_URL);
#endif
    if (projectUrl[0] != L'\0') {
        AddLabel(y, std::wstring(Strings::DlgAboutProjectUrl) + L" " + std::wstring(projectUrl)); y += 24;
    }

    y += 8;
    // Copy diagnostics button
    HWND hCopyBtn = CreateWindowExW(0, L"BUTTON", Strings::DlgAboutCopyDiag.data(),
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                    16, y, 180, 32, hwnd,
                                    reinterpret_cast<HMENU>(IDC_ABOUT_COPY_DIAG),
                                    GetModuleHandleW(nullptr), nullptr);
    // Close button
    HWND hCloseBtn = CreateWindowExW(0, L"BUTTON", Strings::DlgAboutClose.data(),
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                     208, y, 80, 32, hwnd,
                                     reinterpret_cast<HMENU>(IDC_ABOUT_CLOSE),
                                     GetModuleHandleW(nullptr), nullptr);

    // Message loop for modal behavior
    EnableWindow(hwndOwner, FALSE);
    MSG msg;
    while (GetMessageW(&msg, hwnd, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            auto ctrlId = LOWORD(msg.wParam);
            if (ctrlId == IDC_ABOUT_CLOSE ||
                ctrlId == IDOK ||
                (ctrlId == IDCANCEL)) {
                break;
            }
            if (ctrlId == IDC_ABOUT_COPY_DIAG) {
                // Copy diagnostics to clipboard
                std::wstring diag = BuildDiagnosticsString();
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    size_t sizeBytes = (diag.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeBytes);
                    if (hGlobal) {
                        wchar_t* p = static_cast<wchar_t*>(GlobalLock(hGlobal));
                        if (p) {
                            memcpy(p, diag.c_str(), sizeBytes);
                            GlobalUnlock(hGlobal);
                            SetClipboardData(CF_UNICODETEXT, hGlobal);
                        } else {
                            GlobalFree(hGlobal);
                        }
                    }
                    CloseClipboard();
                }
                MessageBoxW(hwnd, Strings::DlgAboutCopyDone.data(),
                            Strings::DlgAboutTitle.data(), MB_OK | MB_ICONINFORMATION);
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hwndOwner, TRUE);
    DestroyWindow(hwnd);
}

std::wstring BuildDiagnosticsString() {
    auto& app = App::RunningApp();
    std::wostringstream ss;
    ss << Strings::DiagTitle << L"\n";
    ss << Strings::DiagVersion << Strings::AppVersion << L"\n";
    // OS build
    OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionExW(&osvi)) {
        ss << Strings::DiagOsBuild << osvi.dwBuildNumber << L"\n";
    }
    // Device count (stub; need window ref)
    ss << Strings::DiagDevices << L"0\n";
    // Index entries
    ss << Strings::DiagIndex << L"0\n";
    // Pack folders
    ss << Strings::DiagPacks;
    ss << (app.settings.packRoot.empty() ? std::wstring(L"(none)")
                                         : app.settings.packRoot.wstring()) << L"\n";
    // Online
    ss << Strings::DiagOnline;
    if (app.settings.onlineManifestUrl.empty()) {
        ss << Strings::DiagOffline;
    } else {
        ss << app.settings.onlineManifestUrl;
    }
    ss << L"\n";
    // Theme
    ss << Strings::DiagTheme << app.settings.theme << L"\n";
    // Elevated
    ss << Strings::DiagElevated << L"\n";

    return ss.str();
}

// --- Confirm install dialog ---
void ShowConfirmWindow(HWND hwndOwner,
                       const std::vector<std::pair<std::wstring, std::wstring>>& drivers) {
    std::wostringstream msg;
    msg << L"Review each version and signature before continuing. Windows may require a restart.\n\n";
    for (auto& drv : drivers) {
        msg << L"• " << drv.first << L"  " << drv.second << L"\n";
    }
    const std::wstring body = msg.str();
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = hwndOwner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    config.pszWindowTitle = Strings::DlgConfirmTitle.data();
    config.pszMainInstruction = L"Confirm driver installation";
    config.pszContent = body.c_str();
    config.pszMainIcon = TD_WARNING_ICON;
    config.pfCallback = DarkTaskDialogCallback;
    int pressed = IDNO;
    if (SUCCEEDED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)) && pressed == IDYES) {
        // Proceed with install
        ::PostMessageW(hwndOwner, WM_APP_BEGIN_INSTALL, 0, 0);
    }
}

} // namespace

void ShowConfirmInstall(HWND hwndOwner,
                        const std::vector<std::pair<std::wstring, std::wstring>>& drivers) {
    ShowConfirmWindow(hwndOwner, drivers);
}

void ShowSettingsDialog(HWND hwndOwner) {
    ShowSettingsWindow(hwndOwner);
}

void ShowAboutDialog(HWND hwndOwner) {
    ShowAboutWindow(hwndOwner);
}

void ShowUndoDialog(HWND hwndOwner) {
    // No previous package is persisted, so this control offers honest
    // recovery guidance rather than claiming it can roll back a driver.
    std::wostringstream msg;
    msg << L"DriverNest has no saved rollback package for this session.\n\n"
           L"To recover a driver, open Device Manager, choose the device's Properties, "
           L"then use the Driver tab's Roll Back Driver option. If that option is unavailable, "
           L"use a Windows restore point.";
    MessageBoxW(hwndOwner, msg.str().c_str(),
                L"Driver recovery",
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

void ShowInstallProgress(HWND hwndOwner, InstallProgress& /*progress*/) {
    // Stub; will implement a real progress dialog later.
}

void UpdateInstallProgress(InstallProgress& /*progress*/) {
    // Stub
}

void CloseInstallProgress() {
    // Stub
}

std::wstring FormatErrorMessage(DWORD errorCode) {
    wchar_t buf[512];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, errorCode, 0, buf, ARRAYSIZE(buf), nullptr);
    std::wstring msg(buf);
    // Trim trailing newlines
    while (!msg.empty() && (msg.back() == L'\n' || msg.back() == L'\r')) {
        msg.pop_back();
    }
    return msg;
}

std::wstring FormatVersion(std::wstring_view version) {
    return std::wstring(version);
}

} // namespace DriverNest::Ui
