#pragma once
#include <windows.h>
#include <string>
#include <string_view>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Precision Instrumentation Color Palette
// Calibrated for deep contrast, zero visual noise, and aerospace-grade clarity.
// All values are native GDI COLORREFs: RGB(r, g, b).
// ---------------------------------------------------------------------------
struct ThemePalette {
    COLORREF background;        // Main window / deep chassis backdrop
    COLORREF surface;           // Deck / listview / panel base
    COLORREF surfaceRaised;     // Toolbar / header / input background
    COLORREF surfaceHover;      // Row / button hover highlight
    COLORREF surfaceActive;     // Button press / active state
    COLORREF text;              // Primary readout text (#F1F5F9 / #0F172A)
    COLORREF textSecondary;     // Telemetry & muted metadata (#94A3B8 / #64748B)
    COLORREF textPlaceholder;   // Placeholder search cue
    COLORREF accent;            // Precision cobalt electric blue (#2563EB)
    COLORREF accentHover;       // Hover state on accent
    COLORREF accentPressed;     // Pressed state on accent
    COLORREF accentLight;       // Signal badge background for updates
    COLORREF success;           // Telemetry emerald (#10B981)
    COLORREF successBg;         // Verified badge background
    COLORREF warning;           // Signal safety amber (#F59E0B)
    COLORREF warningBg;         // Reboot pending badge background
    COLORREF error;             // Diagnostic fault coral (#EF4444)
    COLORREF errorBg;           // Fault badge background
    COLORREF info;              // Telemetry cyan (#0284C7)
    COLORREF infoBg;            // Info badge background
    COLORREF border;            // Structural rule lines (#2A303C / #E2E8F0)
    COLORREF borderFocus;       // Focused element border
    COLORREF selection;         // Row selection highlight
    COLORREF selectionText;     // Selected row primary text
    COLORREF white;
    COLORREF black;
};

enum class ThemeMode {
    System,
    Light,
    Dark,
};

struct Theme {
    ThemeMode mode = ThemeMode::System;
    bool dark = false;
    ThemePalette palette{};

    // Typography
    HFONT fontTitle = nullptr;     // 13pt bold - Application header
    HFONT fontSubtitle = nullptr;  // 8.5pt regular - Tagline / category
    HFONT fontBold = nullptr;      // 9pt semibold - Card headers / button labels
    HFONT fontRegular = nullptr;   // 9pt regular - Primary device labels
    HFONT fontSmall = nullptr;     // 8pt regular - Metadata & small labels
    HFONT fontMono = nullptr;      // 8.5pt tabular - Versions, IDs, INF paths

    // Solid Brushes
    HBRUSH brushBackground = nullptr;
    HBRUSH brushSurface = nullptr;
    HBRUSH brushSurfaceRaised = nullptr;
    HBRUSH brushSurfaceHover = nullptr;
    HBRUSH brushAccent = nullptr;

    // Pens
    HPEN penBorder = nullptr;
    HPEN penAccent = nullptr;

    Theme();
    ~Theme();

    void Refresh(HWND hwndForAccent = nullptr);
    void CleanupGdi();

    static void SetDarkMode(HWND hwnd, bool dark);
    static COLORREF SystemAccentColor();
    static bool SystemIsDark();
    static bool ModeIsDark(ThemeMode mode, bool systemDark);

    static COLORREF Blend(COLORREF c1, COLORREF c2, float t);

    // Signature UI Drawing Primitives
    static void DrawModernCheckbox(HDC hdc, int x, int y, int size, bool checked, bool hover, bool disabled = false);
    static void DrawSignalBadge(HDC hdc, const RECT& rc, std::wstring_view text, COLORREF bgCol, COLORREF textCol, int dotType = 0);
    static void DrawTelemetryProgressBar(HDC hdc, const RECT& rc, int percent, COLORREF barCol, COLORREF bgCol);
    static void DrawScanlineSkeleton(HDC hdc, const RECT& rc, float phase);
    static void DrawFrame(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF borderCol, int radius = 4);
    static void DrawCategoryIcon(HDC hdc, int x, int y, std::wstring_view classGuid, COLORREF color);
};

Theme& CurrentTheme();
void ApplyThemeToWindow(HWND hwnd);

} // namespace DriverNest::Ui
