#pragma once
#include <windows.h>
#include <string>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Palette structure used throughout the UI for paint callbacks, custom draw,
// and control colors. All colors are COLORREF for simplicity in Win32 custom
// draw paths; alpha is ignored.
// ---------------------------------------------------------------------------
struct ThemePalette {
    COLORREF background;     // main window background
    COLORREF surface;        // panels, list background
    COLORREF surfaceRaised;  // buttons, inputs, headers
    COLORREF text;           // primary text
    COLORREF textSecondary;  // secondary / muted text
    COLORREF accent;         // accent color (from system or brand)
    COLORREF success;
    COLORREF warning;
    COLORREF error;
    COLORREF border;         // subtle border between surfaces
    COLORREF selection;      // list selection background (accent at low alpha)
    COLORREF selectionText;

    // Pre-multiplied white/black for common overlays.
    COLORREF white;
    COLORREF black;
};

// ---------------------------------------------------------------------------
// Theme mode requested by the user (persisted in settings.ini as "Theme").
// "System" means we follow the system setting.
// ---------------------------------------------------------------------------
enum class ThemeMode {
    System,
    Light,
    Dark,
};

// ---------------------------------------------------------------------------
// Global theme state. Initialize once at startup; refresh on WM_SETTINGCHANGE
// with the ImmersiveColorSet event.
// ---------------------------------------------------------------------------
struct Theme {
    ThemeMode mode = ThemeMode::System;
    bool dark = false;           // effective dark mode after system override
    ThemePalette palette{};

    // Initialize from registry + system colors. Call at startup and on settings
    // change.
    void Refresh(HWND hwndForAccent = nullptr);

    // Apply the DWMM */dark mode attribute to a window.
    static void SetDarkMode(HWND hwnd, bool dark);

    // Retrieve the effective system accent color (DwmGetColorizationColor).
    static COLORREF SystemAccentColor();

    // Detect system dark mode from HKCU\...\AppsUseLightTheme.
    static bool SystemIsDark();

    // Convert the user's requested theme into an effective dark flag.
    static bool ModeIsDark(ThemeMode mode, bool systemDark);
};

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
Theme& CurrentTheme();

// ---------------------------------------------------------------------------
// Apply palette to a HWND via SetWindowTheme + DWM attributes where supported.
// Call after creating a window or after a theme change.
// ---------------------------------------------------------------------------
void ApplyThemeToWindow(HWND hwnd);

// ---------------------------------------------------------------------------
// Theme-dependent string resources are centralized in Strings.h; this header
// only covers colors and dark/light state.
// ---------------------------------------------------------------------------
} // namespace DriverNest::Ui
