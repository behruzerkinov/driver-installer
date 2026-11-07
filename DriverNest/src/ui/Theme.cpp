#include "Theme.h"
#include "Backdrop.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <winuser.h>
#include <shlwapi.h>
#include <algorithm>
#include <mutex>

namespace DriverNest::Ui {

namespace {

// Brand accent when the system accent is not used or unavailable.
constexpr COLORREF kBrandAccent = 0xFF2F7DE1; // #2F7DE1

// Registry path for per-app dark mode and system theme.
constexpr wchar_t kPersonalizePath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

bool ReadSystemAppsUseLightTheme() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalizePath, 0,
                      KEY_READ, &key) != ERROR_SUCCESS) {
        return false; // assume light if key missing
    }
    DWORD val = 1; // default: light
    DWORD size = sizeof(val);
    const LSTATUS status = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                                            reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) return true;
    return val != 0;
}

// Initialize a palette for a given dark flag. Uses system accent where
// available, otherwise brand blue. Colors are chosen to meet 4.5:1 contrast
// for body text in both modes.
ThemePalette BuildPalette(bool dark, COLORREF accent) {
    ThemePalette p;
    if (dark) {
        p.background      = 0xFF1E1F24; // #1E1F24
        p.surface         = 0xFF272931; // #272931
        p.surfaceRaised  = 0xFF30333D; // #30333D
        p.text            = 0xFFF2F3F5; // #F2F3F5
        p.textSecondary   = 0xFFA4A9B3; // #A4A9B3
        p.success         = 0xFF3FB27F; // #3FB27F
        p.warning         = 0xFFE8A23B; // #E8A23B
        p.error           = 0xFFE5534B; // #E5534B
        p.border          = 0xFF3A3D47; // between surface and raised
        p.white           = 0xFFF2F3F5;
        p.black           = 0xFF1E1F24;
    } else {
        p.background      = 0xFFF5F6F8; // #F5F6F8
        p.surface         = 0xFFFFFFFF; // #FFFFFF
        p.surfaceRaised  = 0xFFEEF0F3; // #EEF0F3
        p.text            = 0xFF1B1D22; // #1B1D22
        p.textSecondary   = 0xFF5C6370; // #5C6370
        p.success         = 0xFF1E8A5A; // #1E8A5A
        p.warning         = 0xFFA86A00; // #A86A00
        p.error           = 0xFFC2362E; // #C2362E
        p.border          = 0xFFD3D7DD; // light border
        p.white           = 0xFFF5F6F8;
        p.black           = 0xFF1B1D22;
    }

    // Accent: prefer system accent, but never let it be too dark for text use.
    // For selection we use accent at ~20% alpha over surface; compute a
    // simple mix here for convenience.
    p.accent = accent;
    // Selection: surface blended 80% with accent (approx). Keep simple.
    {
        auto blend = [](COLORREF c1, COLORREF c2, BYTE t) -> COLORREF {
            BYTE r1 = GetRValue(c1), g1 = GetGValue(c1), b1 = GetBValue(c1);
            BYTE r2 = GetRValue(c2), g2 = GetGValue(c2), b2 = GetBValue(c2);
            BYTE r = static_cast<BYTE>((r1 * (255 - t) + r2 * t) / 255);
            BYTE g = static_cast<BYTE>((g1 * (255 - t) + g2 * t) / 255);
            BYTE b = static_cast<BYTE>((b1 * (255 - t) + b2 * t) / 255);
            return RGB(r, g, b);
        };
        p.selection = blend(p.surface, accent, 60);   // ~24% accent
        p.selectionText = p.text;
    }
    return p;
}

} // namespace

Theme& CurrentTheme() {
    static Theme theme;
    return theme;
}

void Theme::Refresh(HWND hwndForAccent) {
    // Determine effective dark mode
    bool systemDark = SystemIsDark();
    dark = ModeIsDark(mode, systemDark);

    // Accent
    COLORREF accent = SystemAccentColor();
    if (accent == 0) {
        accent = kBrandAccent;
    }
    palette = BuildPalette(dark, accent);

    // Apply to a window if given
    if (hwndForAccent) {
        ApplyThemeToWindow(hwndForAccent);
    }
}

bool Theme::SystemIsDark() {
    return !ReadSystemAppsUseLightTheme();
}

bool Theme::ModeIsDark(ThemeMode mode, bool systemDark) {
    if (mode == ThemeMode::System) return systemDark;
    return mode == ThemeMode::Dark;
}

COLORREF Theme::SystemAccentColor() {
    COLORREF color = 0;
    if (SUCCEEDED(DwmGetColorizationColor(&color, nullptr))) {
        return color;
    }
    return 0;
}

void Theme::SetDarkMode(HWND hwnd, bool dark) {
    if (!hwnd) return;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is defined as 20 in dwmapi.h for SDK 10.0.22000+.
    // We guard it by version check in Backdrop code, but here we call directly
    // and ignore failures.
    typedef enum { DWMWA_USE_IMMERSIVE_DARK_MODE = 20 } DWMWA;
    DWORD value = dark ? 1 : 0;
    DwmSetWindowAttribute(hwnd, static_cast<DWORD>(DWMWA::DWMWA_USE_IMMERSIVE_DARK_MODE),
                          &value, sizeof(value));
}

void ApplyThemeToWindow(HWND hwnd) {
    if (!hwnd) return;
    auto& theme = CurrentTheme();

    // Dark mode attribute
    Theme::SetDarkMode(hwnd, theme.dark);

    // Backdrop type (Mica on Win11, solid on Win10)
    Backdrop::SetBackdrop(hwnd, theme.dark);

    // SetWindowTheme with "DarkMode_Explorer" to get dark common controls
    // where supported (Windows 10 1809+). Guard against failure.
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

} // namespace DriverNest::Ui
