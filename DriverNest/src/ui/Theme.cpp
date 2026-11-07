#include "Theme.h"
#include "Backdrop.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <winuser.h>
#include <algorithm>
#include <cmath>

namespace DriverNest::Ui {

namespace {

constexpr COLORREF kBrandAccent = RGB(37, 99, 235); // #2563EB Precision Cobalt
constexpr wchar_t kPersonalizePath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

bool ReadSystemAppsUseLightTheme() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalizePath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD val = 1;
    DWORD size = sizeof(val);
    const LSTATUS status = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                                            reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) return true;
    return val != 0;
}

HFONT CreateUiFont(int pointSize, int weight, bool italic = false, const wchar_t* faceName = L"Segoe UI") {
    HDC hdc = GetDC(nullptr);
    int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);

    int height = -MulDiv(pointSize, logPixelsY, 72);

    return CreateFontW(
        height, 0, 0, 0, weight,
        italic ? TRUE : FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        faceName
    );
}

ThemePalette BuildPalette(bool dark, COLORREF accent) {
    ThemePalette p{};
    p.accent = accent;

    if (dark) {
        p.background        = RGB(15, 17, 21);        // #0F1115 Deep Chassis Slate
        p.surface           = RGB(24, 27, 34);        // #181B22 Deck / Surface
        p.surfaceRaised     = RGB(33, 38, 49);        // #212631 Raised Deck / Toolbar
        p.surfaceHover      = RGB(42, 49, 64);        // #2A3140 Hover State
        p.surfaceActive     = RGB(51, 60, 78);        // #333C4E Active Pressed
        p.text              = RGB(241, 245, 249);     // #F1F5F9 Cool Platinum Primary
        p.textSecondary     = RGB(148, 163, 184);     // #94A3B8 Telemetry Slate Muted
        p.textPlaceholder   = RGB(100, 116, 139);     // #64748B Placeholder
        p.accentHover       = Theme::Blend(accent, RGB(255, 255, 255), 0.16f);
        p.accentPressed     = Theme::Blend(accent, RGB(0, 0, 0), 0.22f);
        p.accentLight       = Theme::Blend(p.surface, accent, 0.24f);
        p.success           = RGB(16, 185, 129);      // #10B981 Telemetry Emerald
        p.successBg         = RGB(18, 48, 36);        // Soft Emerald Backing
        p.warning           = RGB(245, 158, 11);      // #F59E0B Safety Amber
        p.warningBg         = RGB(53, 38, 14);        // Soft Amber Backing
        p.error             = RGB(239, 68, 68);       // #EF4444 Diagnostic Fault
        p.errorBg           = RGB(56, 20, 20);        // Soft Fault Backing
        p.info              = RGB(56, 189, 248);      // #38BDF8 Telemetry Cyan
        p.infoBg            = RGB(16, 40, 58);        // Soft Cyan Backing
        p.border            = RGB(47, 54, 70);        // #2F3646 Hairline Grid
        p.borderFocus       = accent;
        p.selection         = Theme::Blend(p.surface, accent, 0.32f);
        p.selectionText     = RGB(255, 255, 255);
        p.white             = RGB(255, 255, 255);
        p.black             = RGB(0, 0, 0);
    } else {
        p.background        = RGB(244, 246, 249);     // Clean Industrial Alabaster
        p.surface           = RGB(255, 255, 255);     // Pure Surface
        p.surfaceRaised     = RGB(238, 242, 246);     // Precision Panel
        p.surfaceHover      = RGB(228, 234, 242);     // Hover
        p.surfaceActive     = RGB(218, 226, 236);     // Pressed
        p.text              = RGB(15, 23, 42);        // #0F172A Slate 900
        p.textSecondary     = RGB(100, 116, 139);     // #64748B Slate 500
        p.textPlaceholder   = RGB(148, 163, 184);     // #94A3B8
        p.accentHover       = Theme::Blend(accent, RGB(255, 255, 255), 0.12f);
        p.accentPressed     = Theme::Blend(accent, RGB(0, 0, 0), 0.15f);
        p.accentLight       = Theme::Blend(RGB(255, 255, 255), accent, 0.12f);
        p.success           = RGB(16, 185, 129);
        p.successBg         = RGB(236, 253, 245);
        p.warning           = RGB(217, 119, 6);
        p.warningBg         = RGB(254, 243, 199);
        p.error             = RGB(220, 38, 38);
        p.errorBg           = RGB(254, 242, 242);
        p.info              = RGB(2, 132, 199);
        p.infoBg            = RGB(240, 249, 255);
        p.border            = RGB(226, 232, 240);
        p.borderFocus       = accent;
        p.selection         = Theme::Blend(RGB(255, 255, 255), accent, 0.16f);
        p.selectionText     = RGB(15, 23, 42);
        p.white             = RGB(255, 255, 255);
        p.black             = RGB(0, 0, 0);
    }
    return p;
}

} // namespace

Theme::Theme() = default;

Theme::~Theme() {
    CleanupGdi();
}

Theme& CurrentTheme() {
    static Theme theme;
    return theme;
}

void Theme::CleanupGdi() {
    if (fontTitle)    { DeleteObject(fontTitle); fontTitle = nullptr; }
    if (fontSubtitle) { DeleteObject(fontSubtitle); fontSubtitle = nullptr; }
    if (fontBold)     { DeleteObject(fontBold); fontBold = nullptr; }
    if (fontRegular)  { DeleteObject(fontRegular); fontRegular = nullptr; }
    if (fontSmall)    { DeleteObject(fontSmall); fontSmall = nullptr; }
    if (fontMono)     { DeleteObject(fontMono); fontMono = nullptr; }

    if (brushBackground)    { DeleteObject(brushBackground); brushBackground = nullptr; }
    if (brushSurface)       { DeleteObject(brushSurface); brushSurface = nullptr; }
    if (brushSurfaceRaised) { DeleteObject(brushSurfaceRaised); brushSurfaceRaised = nullptr; }
    if (brushSurfaceHover)  { DeleteObject(brushSurfaceHover); brushSurfaceHover = nullptr; }
    if (brushAccent)        { DeleteObject(brushAccent); brushAccent = nullptr; }

    if (penBorder) { DeleteObject(penBorder); penBorder = nullptr; }
    if (penAccent) { DeleteObject(penAccent); penAccent = nullptr; }
}

void Theme::Refresh(HWND hwndForAccent) {
    CleanupGdi();

    bool systemDark = SystemIsDark();
    dark = ModeIsDark(mode, systemDark);

    COLORREF accent = SystemAccentColor();
    if (accent == 0) {
        accent = kBrandAccent;
    }
    palette = BuildPalette(dark, accent);

    // Precision Typography: Segoe UI Variable + Cascadia / Consolas
    fontTitle    = CreateUiFont(13, FW_BOLD, false, L"Segoe UI Variable Display");
    if (!fontTitle) fontTitle = CreateUiFont(13, FW_BOLD, false, L"Segoe UI");

    fontSubtitle = CreateUiFont(8, FW_NORMAL, false, L"Segoe UI Variable Small");
    if (!fontSubtitle) fontSubtitle = CreateUiFont(8, FW_NORMAL, false, L"Segoe UI");

    fontBold     = CreateUiFont(9, FW_SEMIBOLD, false, L"Segoe UI Variable Text");
    if (!fontBold) fontBold = CreateUiFont(9, FW_BOLD, false, L"Segoe UI");

    fontRegular  = CreateUiFont(9, FW_NORMAL, false, L"Segoe UI Variable Text");
    if (!fontRegular) fontRegular = CreateUiFont(9, FW_NORMAL, false, L"Segoe UI");

    fontSmall    = CreateUiFont(8, FW_NORMAL, false, L"Segoe UI Variable Small");
    if (!fontSmall) fontSmall = CreateUiFont(8, FW_NORMAL, false, L"Segoe UI");

    fontMono     = CreateUiFont(9, FW_NORMAL, false, L"Cascadia Code");
    if (!fontMono) fontMono = CreateUiFont(9, FW_NORMAL, false, L"Consolas");

    // Brushes
    brushBackground    = CreateSolidBrush(palette.background);
    brushSurface       = CreateSolidBrush(palette.surface);
    brushSurfaceRaised = CreateSolidBrush(palette.surfaceRaised);
    brushSurfaceHover  = CreateSolidBrush(palette.surfaceHover);
    brushAccent        = CreateSolidBrush(palette.accent);

    // Pens
    penBorder = CreatePen(PS_SOLID, 1, palette.border);
    penAccent = CreatePen(PS_SOLID, 1, palette.accent);

    if (hwndForAccent) {
        ApplyThemeToWindow(hwndForAccent);
    }
}

bool Theme::SystemIsDark() {
    return !ReadSystemAppsUseLightTheme();
}

bool Theme::ModeIsDark(ThemeMode m, bool systemDark) {
    if (m == ThemeMode::System) return systemDark;
    return m == ThemeMode::Dark;
}

COLORREF Theme::SystemAccentColor() {
    DWORD dwmColor = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&dwmColor, &opaque))) {
        BYTE r = static_cast<BYTE>((dwmColor >> 16) & 0xFF);
        BYTE g = static_cast<BYTE>((dwmColor >> 8) & 0xFF);
        BYTE b = static_cast<BYTE>(dwmColor & 0xFF);
        return RGB(r, g, b);
    }
    return 0;
}

COLORREF Theme::Blend(COLORREF c1, COLORREF c2, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    BYTE r1 = GetRValue(c1), g1 = GetGValue(c1), b1 = GetBValue(c1);
    BYTE r2 = GetRValue(c2), g2 = GetGValue(c2), b2 = GetBValue(c2);
    BYTE r = static_cast<BYTE>(r1 * (1.0f - t) + r2 * t);
    BYTE g = static_cast<BYTE>(g1 * (1.0f - t) + g2 * t);
    BYTE b = static_cast<BYTE>(b1 * (1.0f - t) + b2 * t);
    return RGB(r, g, b);
}

void Theme::SetDarkMode(HWND hwnd, bool isDark) {
    if (!hwnd) return;
    typedef enum { DWMWA_USE_IMMERSIVE_DARK_MODE = 20 } DWMWA;
    DWORD value = isDark ? 1 : 0;
    DwmSetWindowAttribute(hwnd, static_cast<DWORD>(DWMWA::DWMWA_USE_IMMERSIVE_DARK_MODE),
                          &value, sizeof(value));
}

void ApplyThemeToWindow(HWND hwnd) {
    if (!hwnd) return;
    auto& theme = CurrentTheme();
    Theme::SetDarkMode(hwnd, theme.dark);
    Backdrop::SetBackdrop(hwnd, theme.dark);
    SetWindowTheme(hwnd, theme.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

// ---------------------------------------------------------------------------
// Signature Precision Drawing Routines
// ---------------------------------------------------------------------------

void Theme::DrawFrame(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF borderCol, int radius) {
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    HPEN bPen = CreatePen(PS_SOLID, 1, borderCol);

    HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, bPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bPen);
    DeleteObject(bgBrush);
}

void Theme::DrawModernCheckbox(HDC hdc, int x, int y, int size, bool checked, bool hover, bool disabled) {
    auto& t = CurrentTheme();
    RECT rc{ x, y, x + size, y + size };

    COLORREF bgCol = checked ? (disabled ? t.palette.textSecondary : t.palette.accent)
                             : (hover ? t.palette.surfaceHover : t.palette.surface);
    COLORREF borderCol = checked ? (disabled ? t.palette.textSecondary : t.palette.accent)
                                 : (hover ? t.palette.accent : t.palette.border);

    DrawFrame(hdc, rc, bgCol, borderCol, 3);

    if (checked) {
        // Crisp vector checkmark polyline
        HPEN tickPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(hdc, tickPen);

        int pad = size / 4;
        POINT pts[3];
        pts[0] = { x + pad, y + size / 2 };
        pts[1] = { x + size / 2 - 1, y + size - pad - 1 };
        pts[2] = { x + size - pad + 1, y + pad + 1 };

        Polyline(hdc, pts, 3);

        SelectObject(hdc, oldPen);
        DeleteObject(tickPen);
    }
}

void Theme::DrawSignalBadge(HDC hdc, const RECT& rc, std::wstring_view text, COLORREF bgCol, COLORREF textCol, int dotType) {
    // 4px subtle rounded corner badge
    DrawFrame(hdc, rc, bgCol, bgCol, 4);

    auto& t = CurrentTheme();
    HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);

    RECT textRc = rc;

    // Optional hardware emitter signal dot
    if (dotType != 0) {
        int dotSize = 6;
        int dotX = rc.left + 8;
        int dotY = rc.top + (rc.bottom - rc.top - dotSize) / 2;
        HBRUSH dotBrush = CreateSolidBrush(textCol);
        HPEN nullPen = CreatePen(PS_NULL, 0, 0);
        HGDIOBJ ob = SelectObject(hdc, dotBrush);
        HGDIOBJ op = SelectObject(hdc, nullPen);
        Ellipse(hdc, dotX, dotY, dotX + dotSize, dotY + dotSize);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(nullPen);
        DeleteObject(dotBrush);
        textRc.left += 14;
    }

    DrawTextW(hdc, text.data(), static_cast<int>(text.size()), &textRc,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

    SelectObject(hdc, oldFont);
}

void Theme::DrawTelemetryProgressBar(HDC hdc, const RECT& rc, int percent, COLORREF barCol, COLORREF bgCol) {
    percent = std::clamp(percent, 0, 100);

    // Track frame
    DrawFrame(hdc, rc, bgCol, bgCol, 3);

    // Active fill segment
    int totalWidth = rc.right - rc.left;
    int fillWidth = (totalWidth * percent) / 100;
    if (fillWidth > 2) {
        RECT fillRc = rc;
        fillRc.right = fillRc.left + fillWidth;
        DrawFrame(hdc, fillRc, barCol, barCol, 3);
    }
}

void Theme::DrawScanlineSkeleton(HDC hdc, const RECT& rc, float phase) {
    auto& t = CurrentTheme();
    // Shimmer oscillation simulating hardware bus scan
    float s = (std::sin(phase) + 1.0f) * 0.5f;
    COLORREF shimmerCol = Blend(t.palette.surfaceRaised, t.palette.surfaceHover, s);

    DrawFrame(hdc, rc, shimmerCol, shimmerCol, 3);
}

void Theme::DrawCategoryIcon(HDC hdc, int x, int y, std::wstring_view classGuid, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);

    // Draw compact hardware telemetry node indicator (14x14)
    RECT iconRc = { x, y, x + 14, y + 14 };

    // Default icon: micro-processor node with pin connectors
    if (classGuid.find(L"4d36e968") != std::wstring_view::npos) {
        // Display / GPU node
        Rectangle(hdc, x + 1, y + 2, x + 13, y + 11);
        MoveToEx(hdc, x + 3, y + 13, nullptr);
        LineTo(hdc, x + 11, y + 13);
    } else if (classGuid.find(L"4d36e972") != std::wstring_view::npos) {
        // Network node: signal broadcast waves
        MoveToEx(hdc, x + 7, y + 12, nullptr);
        LineTo(hdc, x + 7, y + 8);
        Arc(hdc, x + 3, y + 3, x + 11, y + 11, x + 3, y + 5, x + 11, y + 5);
        Arc(hdc, x + 1, y + 1, x + 13, y + 13, x + 1, y + 3, x + 13, y + 3);
    } else {
        // Generic chip / bus node
        RoundRect(hdc, x + 2, y + 2, x + 12, y + 12, 3, 3);
        MoveToEx(hdc, x, y + 4, nullptr); LineTo(hdc, x + 2, y + 4);
        MoveToEx(hdc, x, y + 7, nullptr); LineTo(hdc, x + 2, y + 7);
        MoveToEx(hdc, x, y + 10, nullptr); LineTo(hdc, x + 2, y + 10);
        MoveToEx(hdc, x + 12, y + 4, nullptr); LineTo(hdc, x + 14, y + 4);
        MoveToEx(hdc, x + 12, y + 7, nullptr); LineTo(hdc, x + 14, y + 7);
        MoveToEx(hdc, x + 12, y + 10, nullptr); LineTo(hdc, x + 14, y + 10);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

} // namespace DriverNest::Ui
