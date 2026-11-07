#include "DetailsPane.h"
#include "Resources.h"
#include "DeviceList.h"
#include "Theme.h"
#include "Strings.h"
#include "install/InstallHistory.h"
#include <commctrl.h>
#include <algorithm>
#include <memory>
#include <sstream>

namespace DriverNest::Ui {

namespace {

constexpr int IDC_BTN_COPY_ID = 3010;
constexpr int IDC_BTN_INSTALL_ONE = 3011;

struct State {
    HWND hwndMain = nullptr;
    Hw::Device currentDevice;
    std::wstring candidateInf;
    std::wstring candidateVersion;
    std::wstring signer;
    int score = 0;
    std::vector<std::wstring> reasonTags;
    bool hasDevice = false;
    HWND hCopyBtn = nullptr;
    HWND hInstallBtn = nullptr;
};

State* GetState(HWND hwnd) {
    return reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void SetState(HWND hwnd, State* state) {
    delete GetState(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
}

COLORREF GetClassAccent(std::wstring_view classGuid, const ThemePalette& pal) {
    if (classGuid.find(L"4d36e968") != std::wstring_view::npos) return pal.gpuAccent;
    if (classGuid.find(L"4d36e972") != std::wstring_view::npos) return pal.netAccent;
    if (classGuid.find(L"4d36e96c") != std::wstring_view::npos) return pal.audioAccent;
    if (classGuid.find(L"36fc9e60") != std::wstring_view::npos) return pal.usbAccent;
    return pal.sysAccent;
}

void CopyToClipboard(const std::wstring& text) {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        size_t sizeBytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeBytes);
        if (hGlobal) {
            wchar_t* p = static_cast<wchar_t*>(GlobalLock(hGlobal));
            if (p) {
                memcpy(p, text.c_str(), sizeBytes);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_UNICODETEXT, hGlobal);
            } else {
                GlobalFree(hGlobal);
            }
        }
        CloseClipboard();
    }
}

} // namespace

void DetailsPane::RegisterClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DetailsPane::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = DetailsPane::WindowClass;
    RegisterClassExW(&wc);
}

HWND DetailsPane::Create(HWND hwndParent, int x, int y, int cx, int cy, HWND hwndMain) {
    RegisterClass();
    HWND hwnd = CreateWindowExW(
        0,
        DetailsPane::WindowClass,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, cx, cy,
        hwndParent,
        reinterpret_cast<HMENU>(IDC_DETAILS),
        GetModuleHandleW(nullptr),
        nullptr
    );
    if (hwnd) {
        Init(hwnd, hwndMain);
    }
    return hwnd;
}

void DetailsPane::Init(HWND hwnd, HWND hwndMain) {
    auto* s = new State();
    s->hwndMain = hwndMain;

    s->hCopyBtn = CreateWindowExW(
        0, L"BUTTON", L"Copy ID",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        0, 0, 78, 26,
        hwnd, reinterpret_cast<HMENU>(IDC_BTN_COPY_ID),
        GetModuleHandleW(nullptr), nullptr
    );

    s->hInstallBtn = CreateWindowExW(
        0, L"BUTTON", L"Install Driver",
        WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
        0, 0, 160, 34,
        hwnd, reinterpret_cast<HMENU>(IDC_BTN_INSTALL_ONE),
        GetModuleHandleW(nullptr), nullptr
    );

    SetState(hwnd, s);
}

void DetailsPane::SetDevice(HWND hwnd, const Hw::Device& device) {
    auto* s = GetState(hwnd);
    if (!s) return;
    s->currentDevice = device;
    s->hasDevice = !device.instanceId.empty();
    ShowWindow(s->hCopyBtn, s->hasDevice ? SW_SHOW : SW_HIDE);
    ShowWindow(s->hInstallBtn, (s->hasDevice && !s->currentDevice.candidateInf.empty() &&
                                s->currentDevice.state != Hw::DriverState::PendingRestart) ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void DetailsPane::SetCandidate(HWND hwnd,
                             const Hw::Device& /*device*/,
                             const std::wstring& candidateInf,
                             const std::wstring& candidateVersion,
                             const std::wstring& signer,
                             int score,
                             std::vector<std::wstring> reasonTags) {
    auto* s = GetState(hwnd);
    if (!s) return;
    s->candidateInf = candidateInf;
    s->candidateVersion = candidateVersion;
    s->signer = signer;
    s->score = score;
    s->reasonTags = std::move(reasonTags);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void DetailsPane::RefreshFromSelection(HWND hwndList, HWND hwndDetails, HWND /*hwndMain*/) {
    int sel = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED);
    if (sel == -1) {
        SetDevice(hwndDetails, Hw::Device{});
        SetCandidate(hwndDetails, Hw::Device{}, L"", L"", L"", 0, {});
        return;
    }
    Hw::Device dev = DeviceList::GetDeviceAt(hwndList, sel);
    SetDevice(hwndDetails, dev);
    SetCandidate(hwndDetails, dev, dev.candidateInf, dev.candidateVersion, L"", 0, {});
}

LRESULT CALLBACK DetailsPane::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto* s = GetState(hwnd);

    switch (uMsg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_COPY_ID && s && s->hasDevice) {
                CopyToClipboard(s->currentDevice.instanceId);
            } else if (id == IDC_BTN_INSTALL_ONE && s && s->hasDevice && s->hwndMain) {
                ::PostMessageW(s->hwndMain, WM_APP_BEGIN_INSTALL, 1, 0);
            }
            return 0;
        }

        case WM_SIZE: {
            int cx = LOWORD(lParam);
            int cy = HIWORD(lParam);
            if (s) {
                if (s->hCopyBtn) {
                    SetWindowPos(s->hCopyBtn, nullptr, cx - 100, 142, 82, 26, SWP_NOZORDER);
                }
                if (s->hInstallBtn) {
                    SetWindowPos(s->hInstallBtn, nullptr, 18, cy - 48, 160, 34, SWP_NOZORDER);
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!hdc) return 0;

            RECT rc;
            GetClientRect(hwnd, &rc);
            int cx = rc.right - rc.left;
            int cy = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, cx, cy);
            HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

            auto& t = CurrentTheme();

            // 1. Translucent frosted glass backing over Acrylic
            Theme::DrawGlassCard(memDC, rc, t.palette.surface, 215, 0, 0, false);

            // Left specular vertical splitter divider
            HPEN divPen = CreatePen(PS_SOLID, 1, t.palette.glassBorderSubtle);
            HGDIOBJ oldPen = SelectObject(memDC, divPen);
            MoveToEx(memDC, 0, 0, nullptr);
            LineTo(memDC, 0, cy);
            SelectObject(memDC, oldPen);
            DeleteObject(divPen);

            if (!s || !s->hasDevice) {
                SetBkMode(memDC, TRANSPARENT);
                HGDIOBJ oldFont = SelectObject(memDC, t.fontTitle);
                SetTextColor(memDC, t.palette.textSecondary);

                RECT emptyRc = rc;
                emptyRc.top = cy / 3;
                DrawTextW(memDC, L"Hardware Inspector Idle", -1, &emptyRc, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontRegular);
                emptyRc.top += 30;
                emptyRc.left += 36;
                emptyRc.right -= 36;
                DrawTextW(memDC, L"Select any device card from the navigation deck on the left to examine driver telemetry, candidate packages, and hardware identifiers.",
                          -1, &emptyRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

                SelectObject(memDC, oldFont);
            } else {
                const auto& dev = s->currentDevice;
                bool isWaitingRestart = (dev.state == Hw::DriverState::PendingRestart) ||
                                        Install::InstallHistory::IsWaitingRestart(dev.instanceId, dev.hardwareIds);

                int pad = 16;
                int y = 14;

                // Card 1: Unigram Hero Profile Card
                RECT heroRc = { pad, y, cx - pad, y + 104 };
                Theme::DrawGlassCard(memDC, heroRc, t.palette.surfaceRaised, 220, t.palette.glassBorder, 10, true);

                // Large Squircle Avatar in Hero
                RECT avatarRc = { heroRc.left + 14, heroRc.top + 14, heroRc.left + 14 + 52, heroRc.top + 14 + 52 };
                COLORREF catCol = GetClassAccent(dev.classGuid, t.palette);
                Theme::DrawSquircleAvatar(memDC, avatarRc, dev.classGuid, catCol, 12);

                int heroTextX = avatarRc.right + 14;
                int heroTextW = heroRc.right - heroTextX - 140;

                SetBkMode(memDC, TRANSPARENT);
                HGDIOBJ oldFont = SelectObject(memDC, t.fontTitle);
                SetTextColor(memDC, t.palette.text);

                // Device Title
                RECT titleRc = { heroTextX, heroRc.top + 14, heroTextX + heroTextW, heroRc.top + 38 };
                DrawTextW(memDC, dev.description.c_str(), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Manufacturer & Hardware Class
                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT mfgRc = { heroTextX, heroRc.top + 38, heroTextX + heroTextW, heroRc.top + 56 };
                std::wstring mfgText = (dev.manufacturer.empty() ? std::wstring(L"Microsoft / Generic OEM") : dev.manufacturer);
                DrawTextW(memDC, mfgText.c_str(), -1, &mfgRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                RECT classRc = { heroTextX, heroRc.top + 56, heroTextX + heroTextW, heroRc.top + 74 };
                std::wstring classText = L"GUID: " + dev.classGuid;
                DrawTextW(memDC, classText.c_str(), -1, &classRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Right Status Pill Badge
                RECT statusPillRc = { heroRc.right - 146, heroRc.top + 14, heroRc.right - 14, heroRc.top + 40 };
                if (isWaitingRestart) {
                    Theme::DrawUnigramPill(memDC, statusPillRc, L"Reboot Pending", t.palette.warningBg, t.palette.warning, t.palette.warning, 1);
                } else if (dev.state == Hw::DriverState::Ok) {
                    Theme::DrawUnigramPill(memDC, statusPillRc, L"Up to Date", t.palette.successBg, t.palette.success, t.palette.success, 1);
                } else if (dev.state == Hw::DriverState::Outdated) {
                    Theme::DrawUnigramPill(memDC, statusPillRc, L"Update Ready", t.palette.accentLight, t.palette.accent, t.palette.accent, 1);
                } else {
                    Theme::DrawUnigramPill(memDC, statusPillRc, L"Driver Missing", t.palette.warningBg, t.palette.warning, t.palette.warning, 1);
                }

                y += 114;

                // Card 2: Pending Reboot Warning Banner
                if (isWaitingRestart) {
                    RECT alertRc = { pad, y, cx - pad, y + 68 };
                    Theme::DrawGlassCard(memDC, alertRc, t.palette.warningBg, 220, t.palette.warning, 8, true);

                    SelectObject(memDC, t.fontBold);
                    SetTextColor(memDC, t.palette.warning);
                    RECT alertTitleRc = { alertRc.left + 14, alertRc.top + 8, alertRc.right - 14, alertRc.top + 26 };
                    DrawTextW(memDC, L"System Restart Pending", -1, &alertTitleRc, DT_SINGLELINE | DT_NOPREFIX);

                    SelectObject(memDC, t.fontSmall);
                    SetTextColor(memDC, t.palette.text);
                    RECT alertBodyRc = { alertRc.left + 14, alertRc.top + 28, alertRc.right - 14, alertRc.bottom - 6 };
                    DrawTextW(memDC, L"This driver was installed in the current session. The device node is queued to activate the new driver binary on the next Windows reboot.",
                              -1, &alertBodyRc, DT_WORDBREAK | DT_NOPREFIX);

                    y += 76;
                }

                // Card 3: Two-Column Driver Comparison Bento Deck
                RECT compRc = { pad, y, cx - pad, y + 118 };
                Theme::DrawGlassCard(memDC, compRc, t.palette.surfaceRaised, 210, t.palette.glassBorder, 8, true);

                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT compHeader = { compRc.left + 14, compRc.top + 10, compRc.right - 14, compRc.top + 28 };
                DrawTextW(memDC, L"Driver Telemetry & Candidate Comparison", -1, &compHeader, DT_SINGLELINE | DT_NOPREFIX);

                int colW = (compRc.right - compRc.left - 36) / 2;
                int col2Left = compRc.left + 14 + colW + 14;

                // Column 1: Installed
                RECT c1Rc = { compRc.left + 14, compRc.top + 34, compRc.left + 14 + colW, compRc.bottom - 10 };
                Theme::DrawGlassCard(memDC, c1Rc, t.palette.surface, 190, t.palette.glassBorderSubtle, 6, false);

                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c1Tag = { c1Rc.left + 10, c1Rc.top + 8, c1Rc.right - 10, c1Rc.top + 24 };
                DrawTextW(memDC, L"ACTIVE IN DRIVER STORE", -1, &c1Tag, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.text);
                RECT c1Ver = { c1Rc.left + 10, c1Rc.top + 26, c1Rc.right - 10, c1Rc.top + 46 };
                std::wstring v1 = L"v" + (dev.installedVersion.empty() ? std::wstring(L"(none)") : dev.installedVersion);
                DrawTextW(memDC, v1.c_str(), -1, &c1Ver, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c1Inf = { c1Rc.left + 10, c1Rc.top + 46, c1Rc.right - 10, c1Rc.bottom - 4 };
                std::wstring inf1 = L"INF: " + (dev.installedInf.empty() ? std::wstring(L"—") : dev.installedInf);
                DrawTextW(memDC, inf1.c_str(), -1, &c1Inf, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Column 2: Candidate
                RECT c2Rc = { col2Left, compRc.top + 34, compRc.right - 14, compRc.bottom - 10 };
                Theme::DrawGlassCard(memDC, c2Rc, t.palette.surface, 190, t.palette.glassBorderSubtle, 6, false);

                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c2Tag = { c2Rc.left + 10, c2Rc.top + 8, c2Rc.right - 10, c2Rc.top + 24 };
                DrawTextW(memDC, L"PACK STAGED CANDIDATE", -1, &c2Tag, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, (dev.state == Hw::DriverState::Outdated) ? t.palette.accent : t.palette.text);
                RECT c2Ver = { c2Rc.left + 10, c2Rc.top + 26, c2Rc.right - 10, c2Rc.top + 46 };
                std::wstring v2 = isWaitingRestart ? L"Installed this session" :
                                 (dev.candidateVersion.empty() ? L"No verified candidate" : L"v" + dev.candidateVersion);
                DrawTextW(memDC, v2.c_str(), -1, &c2Ver, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c2Signer = { c2Rc.left + 10, c2Rc.top + 46, c2Rc.right - 10, c2Rc.bottom - 4 };
                const wchar_t* signatureText = dev.candidateInf.empty() ? L"Catalog: none" :
                                               dev.candidateSigned ? L"Catalog: verified" : L"Catalog: unsigned";
                DrawTextW(memDC, signatureText, -1, &c2Signer, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                y += 128;

                // Card 4: Hardware Identification Deck
                RECT idCardRc = { pad, y, cx - pad, y + 100 };
                Theme::DrawGlassCard(memDC, idCardRc, t.palette.surfaceRaised, 210, t.palette.glassBorder, 8, true);

                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT idTitle = { idCardRc.left + 14, idCardRc.top + 10, idCardRc.right - 96, idCardRc.top + 28 };
                DrawTextW(memDC, L"Hardware Device Instance ID", -1, &idTitle, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT instRc = { idCardRc.left + 14, idCardRc.top + 30, idCardRc.right - 96, idCardRc.top + 48 };
                DrawTextW(memDC, dev.instanceId.c_str(), -1, &instRc, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT hwTitle = { idCardRc.left + 14, idCardRc.top + 52, idCardRc.right - 14, idCardRc.top + 70 };
                DrawTextW(memDC, L"Primary Hardware ID", -1, &hwTitle, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.textSecondary);
                std::wstring firstHwId = dev.hardwareIds.empty() ? L"(none reported)" : dev.hardwareIds[0];
                RECT hwIdRc = { idCardRc.left + 14, idCardRc.top + 72, idCardRc.right - 14, idCardRc.top + 92 };
                DrawTextW(memDC, firstHwId.c_str(), -1, &hwIdRc, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                SelectObject(memDC, oldFont);
            }

            BitBlt(hdc, 0, 0, cx, cy, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            delete s;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

} // namespace DriverNest::Ui
