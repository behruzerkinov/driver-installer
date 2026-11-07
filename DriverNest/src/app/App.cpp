#include "App.h"
#include "MainWindow.h"
#include "Theme.h"
#include <windows.h>
#include <sddl.h>
#include <lmcons.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace DriverNest::App {

namespace {

// Check whether the current process is running as administrator.
bool IsElevated() {
    BOOL isAdmin = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        // CheckTokenMembership against the Administrators SID
        PSID adminSid = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2,
                                     SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS,
                                     0, 0, 0, 0, 0, 0, &adminSid)) {
            if (CheckTokenMembership(nullptr, adminSid, &isAdmin)) {
                // isAdmin is correct; nothing to do.
            }
            FreeSid(adminSid);
        }
        CloseHandle(token);
    }
    return isAdmin;
}

// Ensure only one DriverNest instance is running.
bool EnsureSingleInstance(const std::wstring& windowClass) {
    // Named mutex visible to all sessions.
    HANDLE mutex = CreateMutexW(nullptr, FALSE,
                                L"Global\\DriverNest.SingleInstance");
    if (!mutex) {
        // Could not create; likely another instance. Try to find and focus it.
        HWND existing = FindWindowW(windowClass.c_str(), nullptr);
        if (existing) {
            // Restore if minimized and bring to foreground
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
            // Flash taskbar as well
            FLASHWINFO fi{};
            fi.cbSize = sizeof(fi);
            fi.hwnd = existing;
            fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
            fi.uCount = 2;
            fi.dwTimeout = 0;
            FlashWindowEx(&fi);
            return false; // we exit
        }
        // No existing window found; maybe a stale mutex. Recreate.
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // Another instance holds it; still treat as single-instance.
            return false;
        }
    }
    return true;
}

} // namespace

App& RunningApp() {
    static App instance;
    return instance;
}

bool App::Initialize(int argc, wchar_t* argv[]) {
    // Parse CLI flags
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--debug" || arg == L"-d") {
            debug_ = true;
        }
    }

    // Elevation check
    if (!IsElevated()) {
        // Show a message box and exit.
        MessageBoxW(nullptr,
                    L"DriverNest requires administrator privileges to enumerate "
                    L"devices and install drivers.\n\nPlease run as administrator.",
                    L"DriverNest",
                    MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
        return false;
    }

    // Single instance
    windowClass_ = L"DriverNestMainWndClass";
    if (!EnsureSingleInstance(windowClass_)) {
        return false; // another instance is running
    }

    // Create app data directory (with ACL)
    if (!CreateAppDataDirectory()) {
        // Non-fatal warning; the app can still run but settings/logs may fail.
        MessageBoxW(nullptr,
                    L"Could not create the application data folder "
                    L"(%LOCALAPPDATA%\\DriverNest). Some features may not work.",
                    L"DriverNest",
                    MB_ICONWARNING | MB_OK | MB_SETFOREGROUND);
    }

    // Logging
    auto logPath = AppDataDirectory() / "driver_nest.log";
    LogLevel level = debug_ ? LogLevel::Debug : LogLevel::Info;
    LogInitialize(logPath, level, debug_);
    LOG_INFO(L"DriverNest starting. Version 1.0.0. Debug=" +
             std::wstring(debug_ ? L"true" : L"false"));

    // Load settings
    settings = LoadSettings();

    // Apply system theme hint at startup
    Ui::CurrentTheme().Refresh(mainWindow);
    Ui::ApplyThemeToWindow(mainWindow);

    return true;
}

int App::Run() {
    // Create main window
    Ui::MainWindow wnd;
    wnd.Create(settings);
    mainWindow = wnd.Hwnd();
    if (!mainWindow) {
        LOG_FATAL(L"Failed to create the main window.");
        return 1;
    }

    LOG_INFO(L"Main window created.");

    // Message loop
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    LOG_INFO(L"Message loop ended.");
    return static_cast<int>(msg.wParam);
}

void App::OnDestroy(HWND /*hwnd*/) {
    PostQuitMessage(0);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // This is a static entry point that forwards to the active MainWindow.
    return Ui::MainWindow::HandleMessage(hwnd, uMsg, wParam, lParam);
}

} // namespace DriverNest::App
