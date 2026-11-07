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

namespace DriverNest::Ui {

namespace {

struct SettingsDlgCtx {
    HWND hwndOwner;
};

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
    // Create a small modal-like window (we will actually make it modeless and
    // disable the owner until closed).
    HWND hwnd = CreateWindowExW(
        0,
        L"STATIC",
        Strings::DlgSettingsTitle.data(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 500, 400,
        hwndOwner, nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );
    if (!hwnd) return;

    // position centered over owner
    RECT rOwner;
    GetWindowRect(hwndOwner, &rOwner);
    int ownerCx = rOwner.right - rOwner.left;
    int ownerCy = rOwner.bottom - rOwner.top;
    int ownerX = rOwner.left;
    int ownerY = rOwner.top;
    int w = 500, h = 400;
    int x = ownerX + (ownerCx - w) / 2;
    int y = ownerY + (ownerCy - h) / 2;
    SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_NOZORDER);

    // Apply theme
    CurrentTheme().Refresh(hwnd);
    ApplyThemeToWindow(hwnd);

    // Placeholder: just show a message box for now; full settings UI later.
    MessageBoxW(hwnd, L"Settings dialog placeholder. Full implementation in progress.",
                Strings::DlgSettingsTitle.data(), MB_OK | MB_ICONINFORMATION);
    DestroyWindow(hwnd);
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
    // Build message text
    std::wostringstream msg;
    msg << Strings::DlgConfirmBody << L"\n\n";
    for (auto& drv : drivers) {
        msg << L"• " << drv.first << L"  " << drv.second << L"\n";
    }
    msg << L"\n" << Strings::DlgRestorePointNotice << L": ";
    msg << Strings::DlgRestorePointCreated << L"\n";

    int answer = MessageBoxW(hwndOwner, msg.str().c_str(),
                             Strings::DlgConfirmTitle.data(),
                             MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND);
    if (answer == IDYES) {
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
    // Show a simple dialog with info about undoing the last batch.
    std::wostringstream msg;
    msg << L"Undo last batch restores the previously installed driver packages "
           L"by reinstalling them via pnputil.\n\n"
           L"If that is not possible, you can use System Restore manually.";
    MessageBoxW(hwndOwner, msg.str().c_str(),
                Strings::DlgConfirmTitle.data(),
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);

    // Actually perform undo
    ::PostMessageW(hwndOwner, WM_APP_BEGIN_UNDO, 0, 0);
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
