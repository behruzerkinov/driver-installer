#include "Backdrop.h"
#include "Theme.h"
#include <dwmapi.h>
#include <versionhelpers.h>

namespace DriverNest::Ui {

namespace {

// Guard against calling DwmSetWindowAttribute with DWMWA_SYSTEMBACKDROP_TYPE
// on builds that do not support it. We use RtlGetVersion for an exact build
// number check, since IsWindows11OrGreater() may not be reliable for the
// exact threshold (22000).
bool IsBuildAtLeast(std::uint32_t build) {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi))) {
        return osvi.dwBuildNumber >= build;
    }
    return false;
}

} // namespace

void Backdrop::SetBackdrop(HWND hwnd, bool dark) {
    if (!hwnd) return;
    if (!MicaAvailable()) {
        // Solid fallback is handled during WM_ERASEBKGND; nothing else to do.
        return;
    }

    // On Windows 11 build 22000 and up, set Mica.
    // DWMWA_SYSTEMBACKDROP_TYPE is value 34; Mica is 2.
    typedef enum { DWMWA_SYSTEMBACKDROP_TYPE = 34 } DWMWA;
    // Mica = 2, Acrylic = 3 (not used), Tabbed = 4, None = 1
    DWORD backdropType = 2; // Mica
    HRESULT hr = DwmSetWindowAttribute(hwnd, static_cast<DWORD>(DWMWA::DWMWA_SYSTEMBACKDROP_TYPE),
                                       &backdropType, sizeof(backdropType));
    if (FAILED(hr)) {
        // Fallback will handle painting in WM_ERASEBKGND.
    }
}

BOOL Backdrop::PaintSolidBackground(HWND hwnd, HDC hdc) {
    auto& theme = CurrentTheme();
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return FALSE;

    // Fill with surface color
    HBRUSH brush = CreateSolidBrush(theme.palette.surface);
    if (!brush) return FALSE;
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    return TRUE;
}

bool Backdrop::MicaAvailable() {
    return IsBuildAtLeast(22000);
}

} // namespace DriverNest::Ui
