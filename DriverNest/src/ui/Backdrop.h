#pragma once
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Window backdrop support.
//
// Windows 11 (build 22000+): use DWMWA_SYSTEMBACKDROP_TYPE with Mica (value 2).
// Windows 10: solid surface color via WM_ERASEBKGND painting and the theme
// palette. Acrylic is not used per spec.
//
// The caller must ensure the window's background brush is null (or a solid
// color) and that WM_SIZE/theme changes trigger a repaint.
// ---------------------------------------------------------------------------
struct Backdrop {
    // Set the backdrop for a window. No-op on older builds.
    static void SetBackdrop(HWND hwnd, bool dark);

    // Paint the backdrop in WM_ERASEBKGND for solid fallback.
    // Returns TRUE if the background was handled.
    static BOOL PaintSolidBackground(HWND hwnd, HDC hdc);

    // Determine if Mica is available on this system.
    static bool MicaAvailable();
};

} // namespace DriverNest::Ui
