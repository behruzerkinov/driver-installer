#pragma once
#include "Settings.h"
#include "Log.h"
#include <string>
#include <memory>
#include <windows.h>

namespace DriverNest::App {

// ---------------------------------------------------------------------------
// Application bootstrap and global services.
//
// Responsibilities:
//  - Parse CLI args (only --debug is recognized in v1).
//  - Check elevation (must be admin).
//  - Enforce single instance via a named mutex.
//  - Initialize logging.
//  - Create and run the main window.
// ---------------------------------------------------------------------------
struct App {
    Settings settings;
    HWND mainWindow = nullptr;

    // Returns false when the app should exit without showing a window (e.g.,
    // not elevated, or another instance is already running).
    [[nodiscard]] bool Initialize(int argc, wchar_t* argv[]);

    // Run the message loop. Blocks until the main window closes.
    int Run();

    // Called on WM_DESTROY to post a quit message.
    static void OnDestroy(HWND hwnd);

    // Window procedure for the main window.
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    std::wstring windowClass_;           // registered class name
    bool debug_ = false;
};

// Global accessor for the running app (used by dialogs and workers).
App& RunningApp();

} // namespace DriverNest::App
