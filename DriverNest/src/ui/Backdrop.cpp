#include "Backdrop.h"
#include "Theme.h"
#include <dwmapi.h>
#include <cstdint>

namespace DriverNest::Ui {

namespace {

// Internal Undocumented Windows 10/11 Accent API structures for Acrylic blur
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5,
    ACCENT_INVALID_STATE = 6
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

bool IsBuildAtLeast(std::uint32_t build) {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi))) {
        return osvi.dwBuildNumber >= build;
    }
    return false;
}

bool ApplyWindows10Acrylic(HWND hwnd, bool dark) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return false;

    auto SetWindowCompositionAttribute = reinterpret_cast<pfnSetWindowCompositionAttribute>(
        GetProcAddress(hUser, "SetWindowCompositionAttribute"));
    if (!SetWindowCompositionAttribute) return false;

    ACCENT_POLICY policy{};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.AccentFlags = 2; // draw all borders

    // ARGB: 0xAA alpha with deep chassis tint
    if (dark) {
        policy.GradientColor = 0xAA121620; // 66% opacity dark slate
    } else {
        policy.GradientColor = 0xAAF4F6F9; // 66% opacity alabaster
    }

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.Attrib = 19; // WCA_ACCENT_POLICY
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    return SetWindowCompositionAttribute(hwnd, &data) != FALSE;
}

} // namespace

void Backdrop::SetBackdrop(HWND hwnd, bool dark) {
    if (!hwnd) return;

    // 1. Extend DWM frame into client area for seamless borderless glass
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 2. Set Immersive Dark Mode for DWM window frame
    BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

    // 3. Windows 11 22621+: DWMSBT_TRANSIENTWINDOW (Acrylic Blur)
    if (IsBuildAtLeast(22621)) {
        DWORD backdropType = 3; // DWMSBT_TRANSIENTWINDOW (Acrylic Blur)
        HRESULT hr = DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */,
                                           &backdropType, sizeof(backdropType));
        if (SUCCEEDED(hr)) return;
    }

    // 4. Windows 11 22000: DWMWA_MICA_EFFECT
    if (IsBuildAtLeast(22000)) {
        BOOL mica = TRUE;
        HRESULT hr = DwmSetWindowAttribute(hwnd, 1029 /* DWMWA_MICA_EFFECT */, &mica, sizeof(mica));
        if (SUCCEEDED(hr)) return;
    }

    // 5. Windows 10 (1709-22H2) & fallback: SetWindowCompositionAttribute Acrylic
    ApplyWindows10Acrylic(hwnd, dark);
}

BOOL Backdrop::PaintSolidBackground(HWND hwnd, HDC hdc) {
    auto& theme = CurrentTheme();
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return FALSE;

    HBRUSH brush = CreateSolidBrush(theme.palette.background);
    if (!brush) return FALSE;
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    return TRUE;
}

bool Backdrop::AcrylicAvailable() {
    return IsBuildAtLeast(17763); // Windows 10 1809+
}

bool Backdrop::MicaAvailable() {
    return IsBuildAtLeast(22000);
}

} // namespace DriverNest::Ui
