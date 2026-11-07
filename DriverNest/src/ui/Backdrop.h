#pragma once
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Native Windows Acrylic & Aero Glass Backdrop Engine.
// Supports:
// - Windows 11 (22621+): DWMWA_SYSTEMBACKDROP_TYPE = 3 (DWMSBT_TRANSIENTWINDOW Acrylic)
// - Windows 11 (22000): DWMWA_MICA_EFFECT (1029)
// - Windows 10 (1709-22H2): SetWindowCompositionAttribute with ACCENT_ENABLE_ACRYLICBLURBEHIND
// - Full-bleed DWM frame extension (DwmExtendFrameIntoClientArea)
// ---------------------------------------------------------------------------
struct Backdrop {
    // Apply Windows native blurry Acrylic Aero backdrop
    static void SetBackdrop(HWND hwnd, bool dark);

    // Paint the backdrop in WM_ERASEBKGND for translucent fallback
    static BOOL PaintSolidBackground(HWND hwnd, HDC hdc);

    // Capability queries
    static bool AcrylicAvailable();
    static bool MicaAvailable();
};

} // namespace DriverNest::Ui
