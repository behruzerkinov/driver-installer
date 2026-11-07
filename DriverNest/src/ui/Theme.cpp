#include "Theme.h"
#include "Backdrop.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <winuser.h>
#include <algorithm>
#include <cmath>

namespace DriverNest::Ui {

namespace {

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

HFONT CreateUiFont(int pointSize, int weight, bool italic = false, const wchar_t* faceName = L"Segoe UI Variable Text") {
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
        // Aerospace Deep Chassis palette
        p.background        = RGB(15, 17, 21);        // #0F1115 chassis
        p.surface           = RGB(24, 27, 34);        // #181B22 deck
        p.surfaceRaised     = RGB(33, 38, 49);        // #212631 raised surface
        p.surfaceHover      = RGB(43, 50, 64);
        p.surfaceActive     = RGB(52, 62, 81);
        p.text              = RGB(241, 245, 249);     // High-clarity cool white
        p.textSecondary     = RGB(164, 176, 194);
        p.textPlaceholder   = RGB(126, 140, 160);
        p.accentHover       = Theme::Blend(accent, RGB(255, 255, 255), 0.18f);
        p.accentPressed     = Theme::Blend(accent, RGB(0, 0, 0), 0.24f);
        p.accentLight       = Theme::Blend(p.surface, accent, 0.28f);
        p.success           = RGB(16, 185, 129);      // #10B981 Telemetry Emerald
        p.successBg         = RGB(16, 48, 36);        // Soft Emerald Backing
        p.warning           = RGB(245, 158, 11);      // #F59E0B Safety Amber
        p.warningBg         = RGB(56, 40, 16);        // Soft Amber Backing
        p.error             = RGB(239, 68, 68);       // #EF4444 Diagnostic Fault
        p.errorBg           = RGB(60, 24, 24);        // Soft Fault Backing
        p.info              = RGB(56, 189, 248);      // #38BDF8 Telemetry Cyan
        p.infoBg            = RGB(18, 44, 64);        // Soft Cyan Backing
        p.border            = RGB(47, 54, 70);        // #2F3646 structural line
        p.borderFocus       = accent;
        p.selection         = Theme::Blend(p.surface, accent, 0.35f);
        p.selectionText     = RGB(255, 255, 255);

        // Glass Translucency Tokens
        p.glassCard         = p.surface;
        p.glassCardHover    = p.surfaceHover;
        p.glassBorder       = RGB(66, 78, 99);
        p.glassBorderSubtle = p.border;
        p.glassHighlight    = RGB(255, 255, 255);

        // Squircle Category Vibrant Accents
        p.gpuAccent         = RGB(59, 130, 246);      // Cobalt Blue
        p.audioAccent       = RGB(168, 85, 247);      // Electric Purple
        p.netAccent         = RGB(16, 185, 129);      // Emerald Green
        p.usbAccent         = RGB(245, 158, 11);      // Amber Gold
        p.sysAccent         = RGB(6, 182, 212);       // Teal Cyan

        p.white             = RGB(255, 255, 255);
        p.black             = RGB(0, 0, 0);
    } else {
        // Unigram Windows Blurry Aero Light Palette
        p.background        = RGB(240, 243, 248);
        p.surface           = RGB(255, 255, 255);
        p.surfaceRaised     = RGB(242, 245, 250);
        p.surfaceHover      = RGB(232, 238, 246);
        p.surfaceActive     = RGB(220, 228, 240);
        p.text              = RGB(15, 23, 42);
        p.textSecondary     = RGB(100, 116, 139);
        p.textPlaceholder   = RGB(148, 163, 184);
        p.accentHover       = Theme::Blend(accent, RGB(255, 255, 255), 0.15f);
        p.accentPressed     = Theme::Blend(accent, RGB(0, 0, 0), 0.18f);
        p.accentLight       = Theme::Blend(RGB(255, 255, 255), accent, 0.14f);
        p.success           = RGB(16, 185, 129);
        p.successBg         = RGB(220, 252, 231);
        p.warning           = RGB(217, 119, 6);
        p.warningBg         = RGB(254, 243, 199);
        p.error             = RGB(220, 38, 38);
        p.errorBg           = RGB(254, 226, 226);
        p.info              = RGB(2, 132, 199);
        p.infoBg            = RGB(224, 242, 254);
        p.border            = RGB(226, 232, 240);
        p.borderFocus       = accent;
        p.selection         = Theme::Blend(RGB(255, 255, 255), accent, 0.18f);
        p.selectionText     = RGB(15, 23, 42);

        p.glassCard         = RGB(255, 255, 255);
        p.glassCardHover    = RGB(240, 245, 255);
        p.glassBorder       = RGB(210, 220, 235);
        p.glassBorderSubtle = RGB(230, 235, 245);
        p.glassHighlight    = RGB(255, 255, 255);

        p.gpuAccent         = RGB(37, 99, 235);
        p.audioAccent       = RGB(147, 51, 234);
        p.netAccent         = RGB(5, 150, 105);
        p.usbAccent         = RGB(217, 119, 6);
        p.sysAccent         = RGB(8, 145, 178);

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

COLORREF Theme::AccentColor(std::wstring_view name) {
    if (name == L"Violet") return RGB(139, 92, 246);
    if (name == L"Teal") return RGB(20, 184, 166);
    if (name == L"Amber") return RGB(245, 158, 11);
    return RGB(37, 99, 235);
}

std::wstring Theme::NextAccentName(std::wstring_view current) {
    if (current == L"Blue") return L"Violet";
    if (current == L"Violet") return L"Teal";
    if (current == L"Teal") return L"Amber";
    return L"Blue";
}

void Theme::SetAccentColor(COLORREF color) {
    palette.accent = color;
    palette.accentHover = Blend(color, RGB(255, 255, 255), 0.18f);
    palette.accentPressed = Blend(color, RGB(0, 0, 0), 0.24f);
    palette.accentLight = Blend(palette.surface, color, 0.28f);
    palette.borderFocus = color;
    palette.selection = Blend(palette.surface, color, 0.35f);
    if (brushAccent) { DeleteObject(brushAccent); brushAccent = CreateSolidBrush(color); }
    if (penAccent) { DeleteObject(penAccent); penAccent = CreatePen(PS_SOLID, 1, color); }
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

    mode = ThemeMode::Dark;
    dark = true;

    // Keep the product's dark identity stable across Windows appearance changes.
    const COLORREF accent = AccentColor(accentName);
    palette = BuildPalette(dark, accent);

    // Unigram Fluid Typography: Segoe UI Variable Display & Text
    fontTitle    = CreateUiFont(14, FW_BOLD, false, L"Segoe UI Variable Display");
    if (!fontTitle) fontTitle = CreateUiFont(14, FW_BOLD, false, L"Segoe UI");

    fontSubtitle = CreateUiFont(9, FW_NORMAL, false, L"Segoe UI Variable Text");
    if (!fontSubtitle) fontSubtitle = CreateUiFont(9, FW_NORMAL, false, L"Segoe UI");

    fontBold     = CreateUiFont(10, FW_SEMIBOLD, false, L"Segoe UI Variable Text");
    if (!fontBold) fontBold = CreateUiFont(10, FW_BOLD, false, L"Segoe UI");

    fontRegular  = CreateUiFont(10, FW_NORMAL, false, L"Segoe UI Variable Text");
    if (!fontRegular) fontRegular = CreateUiFont(10, FW_NORMAL, false, L"Segoe UI");

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
    if (GetAncestor(hwnd, GA_ROOT) == hwnd) {
        Theme::SetDarkMode(hwnd, theme.dark);
        Backdrop::SetBackdrop(hwnd, theme.dark);
    }
    SetWindowTheme(hwnd, theme.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

// ---------------------------------------------------------------------------
// Unigram Aero Glass Primitives
// ---------------------------------------------------------------------------

void Theme::DrawGlassCard(HDC hdc, const RECT& rc, COLORREF bgCol, BYTE /*alpha*/, COLORREF borderCol, int radius, bool specular) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    radius = std::clamp(radius, 0, 5);
    // Opaque native GDI surfaces avoid blending against uninitialized memory
    // DCs and remove a per-card DIB allocation from every repaint.
    HBRUSH brush = CreateSolidBrush(bgCol);
    HPEN pen = CreatePen(PS_SOLID, 1, borderCol ? borderCol : bgCol);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    if (specular && borderCol && h > 10 && w > radius * 2) {
        HPEN highlight = CreatePen(PS_SOLID, 1, Theme::Blend(borderCol, RGB(255, 255, 255), 0.18f));
        if (highlight) {
            HGDIOBJ oldHighlight = SelectObject(hdc, highlight);
            MoveToEx(hdc, rc.left + radius + 2, rc.top + 1, nullptr);
            LineTo(hdc, rc.right - radius - 2, rc.top + 1);
            SelectObject(hdc, oldHighlight);
            DeleteObject(highlight);
        }
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void Theme::DrawSquircleAvatar(HDC hdc, const RECT& rc, std::wstring_view classGuid, COLORREF accentCol, int radius) {
    // Fill squircle with translucent accent tint
    COLORREF bgCol = Theme::Blend(CurrentTheme().palette.surface, accentCol, 0.22f);
    COLORREF borderCol = accentCol;

    DrawGlassCard(hdc, rc, bgCol, 220, borderCol, radius, true);

    // Centered vector emblem
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;
    DrawCategoryIcon(hdc, cx - 8, cy - 8, classGuid, borderCol);
}

void Theme::DrawUnigramPill(HDC hdc, const RECT& rc, std::wstring_view text, COLORREF bgCol, COLORREF textCol, COLORREF borderCol, int dotType) {
    DrawGlassCard(hdc, rc, bgCol, 210, borderCol != 0 ? borderCol : bgCol, (rc.bottom - rc.top) / 2, true);

    int textX = rc.left + 8;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    if (dotType > 0) {
        HBRUSH dotBrush = CreateSolidBrush(textCol);
        HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, textX, cy - 3, textX + 6, cy + 3);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(dotBrush);
        textX += 10;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    auto& t = CurrentTheme();
    HGDIOBJ oldFont = SelectObject(hdc, t.fontSmall);

    RECT trc = rc;
    trc.left = textX;
    trc.right -= 6;
    DrawTextW(hdc, text.data(), static_cast<int>(text.size()), &trc,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    SelectObject(hdc, oldFont);
}

void Theme::DrawAeroButton(HDC hdc, const RECT& rc, std::wstring_view text, bool isSelected, bool isHover, bool isPrimary, bool isDisabled) {
    auto& t = CurrentTheme();
    COLORREF bgCol;
    COLORREF borderCol;
    COLORREF textCol;

    if (isDisabled) {
        bgCol = t.palette.surface;
        borderCol = t.palette.border;
        textCol = t.palette.textSecondary;
    } else if (isPrimary) {
        bgCol = isSelected ? t.palette.accentPressed : (isHover ? t.palette.accentHover : t.palette.accent);
        borderCol = Theme::Blend(bgCol, RGB(255, 255, 255), 0.35f);
        textCol = RGB(255, 255, 255);
    } else {
        bgCol = isSelected ? t.palette.surfaceActive : (isHover ? t.palette.surfaceHover : t.palette.surfaceRaised);
        borderCol = isHover ? t.palette.accent : t.palette.border;
        textCol = isHover ? t.palette.text : t.palette.textSecondary;
    }

    int radius = (rc.bottom - rc.top) / 2; // Pill button
    if (radius > 12) radius = 8;
    DrawGlassCard(hdc, rc, bgCol, 230, borderCol, radius, true);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    HGDIOBJ oldFont = SelectObject(hdc, isPrimary ? t.fontBold : t.fontRegular);
    RECT trc = rc;
    DrawTextW(hdc, text.data(), static_cast<int>(text.size()), &trc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

void Theme::DrawModernCheckbox(HDC hdc, int x, int y, int size, bool checked, bool hover, bool disabled) {
    auto& t = CurrentTheme();
    RECT rc = { x, y, x + size, y + size };

    COLORREF bgCol;
    COLORREF borderCol;

    if (disabled) {
        bgCol = t.palette.surface;
        borderCol = t.palette.border;
    } else if (checked) {
        bgCol = t.palette.accent;
        borderCol = Theme::Blend(t.palette.accent, RGB(255, 255, 255), 0.3f);
    } else {
        bgCol = hover ? t.palette.surfaceHover : t.palette.surfaceRaised;
        borderCol = hover ? t.palette.accent : t.palette.border;
    }

    DrawGlassCard(hdc, rc, bgCol, 240, borderCol, 4, true);

    if (checked) {
        HPEN checkPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(hdc, checkPen);
        POINT pts[3];
        pts[0] = { x + 3, y + size / 2 };
        pts[1] = { x + size / 2 - 1, y + size - 5 };
        pts[2] = { x + size - 4, y + 4 };
        Polyline(hdc, pts, 3);
        SelectObject(hdc, oldPen);
        DeleteObject(checkPen);
    }
}

void Theme::DrawSignalBadge(HDC hdc, const RECT& rc, std::wstring_view text, COLORREF bgCol, COLORREF textCol, int dotType) {
    DrawUnigramPill(hdc, rc, text, bgCol, textCol, Theme::Blend(bgCol, RGB(255, 255, 255), 0.25f), dotType);
}

void Theme::DrawTelemetryProgressBar(HDC hdc, const RECT& rc, int percent, COLORREF barCol, COLORREF bgCol) {
    DrawFrame(hdc, rc, bgCol, bgCol, (rc.bottom - rc.top) / 2);

    int w = rc.right - rc.left;
    int fillW = std::clamp((w * percent) / 100, 0, w);
    if (fillW > 0) {
        RECT fillRc = { rc.left, rc.top, rc.left + fillW, rc.bottom };
        DrawGlassCard(hdc, fillRc, barCol, 255, Theme::Blend(barCol, RGB(255, 255, 255), 0.3f), (rc.bottom - rc.top) / 2, true);
    }
}

void Theme::DrawScanlineSkeleton(HDC hdc, const RECT& rc, float phase) {
    auto& t = CurrentTheme();
    float s = (std::sin(phase * 4.0f) + 1.0f) * 0.5f;
    COLORREF c = Theme::Blend(t.palette.surfaceRaised, t.palette.surfaceHover, s * 0.7f);
    COLORREF b = Theme::Blend(t.palette.border, t.palette.accent, s * 0.35f);
    DrawGlassCard(hdc, rc, c, 180, b, 6, true);
}

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

void Theme::DrawCategoryIcon(HDC hdc, int x, int y, std::wstring_view classGuid, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    if (classGuid.find(L"4d36e968") != std::wstring_view::npos) {
        // Display / GPU: Monitor frame with stand
        Rectangle(hdc, x, y, x + 16, y + 11);
        MoveToEx(hdc, x + 5, y + 11, nullptr); LineTo(hdc, x + 5, y + 14);
        MoveToEx(hdc, x + 11, y + 11, nullptr); LineTo(hdc, x + 11, y + 14);
        MoveToEx(hdc, x + 3, y + 14, nullptr); LineTo(hdc, x + 13, y + 14);
    } else if (classGuid.find(L"4d36e972") != std::wstring_view::npos) {
        // Network / NIC: Ethernet connector + signals
        Rectangle(hdc, x + 2, y + 3, x + 14, y + 13);
        MoveToEx(hdc, x + 5, y + 13, nullptr); LineTo(hdc, x + 5, y + 15);
        MoveToEx(hdc, x + 8, y + 13, nullptr); LineTo(hdc, x + 8, y + 15);
        MoveToEx(hdc, x + 11, y + 13, nullptr); LineTo(hdc, x + 11, y + 15);
    } else if (classGuid.find(L"4d36e96c") != std::wstring_view::npos) {
        // Audio: Speaker cone
        Rectangle(hdc, x + 2, y + 4, x + 7, y + 12);
        POINT pts[3] = { {x + 7, y + 4}, {x + 13, y + 1}, {x + 13, y + 15} };
        Polygon(hdc, pts, 3);
    } else if (classGuid.find(L"36fc9e60") != std::wstring_view::npos) {
        // USB Controller: Trident branches
        MoveToEx(hdc, x + 8, y + 14, nullptr); LineTo(hdc, x + 8, y + 2);
        MoveToEx(hdc, x + 4, y + 8, nullptr); LineTo(hdc, x + 8, y + 5);
        MoveToEx(hdc, x + 12, y + 8, nullptr); LineTo(hdc, x + 8, y + 5);
        Ellipse(hdc, x + 2, y + 7, x + 5, y + 10);
        Rectangle(hdc, x + 11, y + 7, x + 14, y + 10);
    } else {
        // Microchip / Processor
        Rectangle(hdc, x + 3, y + 3, x + 13, y + 13);
        MoveToEx(hdc, x + 5, y + 1, nullptr); LineTo(hdc, x + 5, y + 3);
        MoveToEx(hdc, x + 8, y + 1, nullptr); LineTo(hdc, x + 8, y + 3);
        MoveToEx(hdc, x + 11, y + 1, nullptr); LineTo(hdc, x + 11, y + 3);
        MoveToEx(hdc, x + 5, y + 13, nullptr); LineTo(hdc, x + 5, y + 15);
        MoveToEx(hdc, x + 8, y + 13, nullptr); LineTo(hdc, x + 8, y + 15);
        MoveToEx(hdc, x + 11, y + 13, nullptr); LineTo(hdc, x + 11, y + 15);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

} // namespace DriverNest::Ui
