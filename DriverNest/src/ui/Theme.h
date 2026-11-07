#pragma once
#include <windows.h>
#include <string>
#include <string_view>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Unigram Aero Glass & Fluent Precision Color Palette
// Calibrated for native Windows blurry Aero / Acrylic glass translucency,
// rich squircle avatars, and specular highlights.
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
    COLORREF accent;            // Fluent electric blue (#0078D4 / #2563EB)
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
    COLORREF border;            // Structural rule lines (#2F3646 / #E2E8F0)
    COLORREF borderFocus;       // Focused element border
    COLORREF selection;         // Row selection highlight
    COLORREF selectionText;     // Selected row primary text

    // Aero Glass Translucency Tokens
    COLORREF glassCard;         // Translucent card fill
    COLORREF glassCardHover;    // Hover translucent fill
    COLORREF glassBorder;       // Specular glass hairline border
    COLORREF glassBorderSubtle; // Subtle card border
    COLORREF glassHighlight;    // Specular top highlight

    // Hardware Category Vibrant Accents (for Unigram Squircles)
    COLORREF gpuAccent;         // Display / GPU (#3B82F6 Cobalt)
    COLORREF audioAccent;       // Audio / Sound (#A855F7 Purple)
    COLORREF netAccent;         // Network / Wi-Fi (#10B981 Emerald)
    COLORREF usbAccent;         // USB / Storage (#F59E0B Amber)
    COLORREF sysAccent;         // Chipset / System (#06B6D4 Cyan)

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
    HFONT fontTitle = nullptr;     // 14pt bold - Application header
    HFONT fontSubtitle = nullptr;  // 8.5pt regular - Tagline / category
    HFONT fontBold = nullptr;      // 10pt semibold - Card headers / button labels
    HFONT fontRegular = nullptr;   // 9.5pt regular - Primary device labels
    HFONT fontSmall = nullptr;     // 8.5pt regular - Metadata & small labels
    HFONT fontMono = nullptr;      // 9pt tabular - Versions, IDs, INF paths

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

    // Signature Unigram Aero Glass Drawing Primitives
    static void DrawGlassCard(HDC hdc, const RECT& rc, COLORREF bgCol, BYTE alpha, COLORREF borderCol, int radius = 8, bool specular = true);
    static void DrawSquircleAvatar(HDC hdc, const RECT& rc, std::wstring_view classGuid, COLORREF accentCol, int radius = 10);
    static void DrawUnigramPill(HDC hdc, const RECT& rc, std::wstring_view text, COLORREF bgCol, COLORREF textCol, COLORREF borderCol = 0, int dotType = 0);
    static void DrawAeroButton(HDC hdc, const RECT& rc, std::wstring_view text, bool isSelected, bool isHover, bool isPrimary, bool isDisabled = false);
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
