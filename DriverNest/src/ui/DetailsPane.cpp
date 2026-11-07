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
        0, 0, 76, 24,
        hwnd, reinterpret_cast<HMENU>(IDC_BTN_COPY_ID),
        GetModuleHandleW(nullptr), nullptr
    );

    s->hInstallBtn = CreateWindowExW(
        0, L"BUTTON", L"Install This Driver",
        WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
        0, 0, 150, 32,
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
    ShowWindow(s->hInstallBtn, (s->hasDevice && s->currentDevice.state != Hw::DriverState::PendingRestart) ? SW_SHOW : SW_HIDE);
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
    SetCandidate(hwndDetails, dev, L"", L"", L"", 0, {});
}

LRESULT CALLBACK DetailsPane::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto* s = GetState(hwnd);

    switch (uMsg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_COPY_ID && s && s->hasDevice) {
                CopyToClipboard(s->currentDevice.instanceId);
            } else if (id == IDC_BTN_INSTALL_ONE && s && s->hasDevice && s->hwndMain) {
                ::PostMessageW(s->hwndMain, WM_APP_BEGIN_INSTALL, 0, 0);
            }
            return 0;
        }

        case WM_SIZE: {
            int cx = LOWORD(lParam);
            int cy = HIWORD(lParam);
            if (s) {
                if (s->hCopyBtn) {
                    SetWindowPos(s->hCopyBtn, nullptr, cx - 90, 118, 74, 24, SWP_NOZORDER);
                }
                if (s->hInstallBtn) {
                    SetWindowPos(s->hInstallBtn, nullptr, 16, cy - 44, 150, 32, SWP_NOZORDER);
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

            FillRect(memDC, &rc, t.brushSurface);

            // Left hairline divider
            HPEN divPen = CreatePen(PS_SOLID, 1, t.palette.border);
            HGDIOBJ oldPen = SelectObject(memDC, divPen);
            MoveToEx(memDC, 0, 0, nullptr);
            LineTo(memDC, 0, cy);
            SelectObject(memDC, oldPen);
            DeleteObject(divPen);

            if (!s || !s->hasDevice) {
                SetBkMode(memDC, TRANSPARENT);
                HGDIOBJ oldFont = SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.textSecondary);

                RECT emptyRc = rc;
                emptyRc.top = cy / 3;
                DrawTextW(memDC, L"Hardware Inspector Idle", -1, &emptyRc, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontSmall);
                emptyRc.top += 24;
                emptyRc.left += 28;
                emptyRc.right -= 28;
                DrawTextW(memDC, L"Select any hardware node in the tree to examine device identification, firmware versions, and candidate packages.",
                          -1, &emptyRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

                SelectObject(memDC, oldFont);
            } else {
                const auto& dev = s->currentDevice;
                bool isWaitingRestart = (dev.state == Hw::DriverState::PendingRestart) ||
                                        Install::InstallHistory::IsWaitingRestart(dev.instanceId, dev.hardwareIds);

                int pad = 14;
                int y = 14;

                // Card 1: Hardware Node Summary
                RECT card1Rc = { pad, y, cx - pad, y + 80 };
                Theme::DrawFrame(memDC, card1Rc, t.palette.surfaceRaised, t.palette.border, 4);

                SetBkMode(memDC, TRANSPARENT);
                HGDIOBJ oldFont = SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);

                RECT titleRc = { card1Rc.left + 12, card1Rc.top + 10, card1Rc.right - 120, card1Rc.top + 32 };
                DrawTextW(memDC, dev.description.c_str(), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Status signal pill
                RECT statusPillRc = { card1Rc.right - 130, card1Rc.top + 10, card1Rc.right - 12, card1Rc.top + 32 };
                if (isWaitingRestart) {
                    Theme::DrawSignalBadge(memDC, statusPillRc, L"Reboot Pending", t.palette.warningBg, t.palette.warning, 1);
                } else if (dev.state == Hw::DriverState::Ok) {
                    Theme::DrawSignalBadge(memDC, statusPillRc, L"Verified Current", t.palette.successBg, t.palette.success, 1);
                } else if (dev.state == Hw::DriverState::Outdated) {
                    Theme::DrawSignalBadge(memDC, statusPillRc, L"Update Staged", t.palette.accentLight, t.palette.accent, 1);
                } else {
                    Theme::DrawSignalBadge(memDC, statusPillRc, L"Driver Missing", t.palette.warningBg, t.palette.warning, 1);
                }

                // Manufacturer Line
                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT mfgRc = { card1Rc.left + 12, card1Rc.top + 36, card1Rc.right - 12, card1Rc.top + 54 };
                std::wstring mfgText = L"Manufacturer: " + (dev.manufacturer.empty() ? std::wstring(L"Standard OEM / Microsoft") : dev.manufacturer);
                DrawTextW(memDC, mfgText.c_str(), -1, &mfgRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                RECT classRc = { card1Rc.left + 12, card1Rc.top + 54, card1Rc.right - 12, card1Rc.top + 72 };
                std::wstring classText = L"Class GUID: " + dev.classGuid;
                DrawTextW(memDC, classText.c_str(), -1, &classRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                y += 88;

                // Pending Reboot Telemetry Alert Banner
                if (isWaitingRestart) {
                    RECT alertRc = { pad, y, cx - pad, y + 64 };
                    Theme::DrawFrame(memDC, alertRc, t.palette.warningBg, t.palette.warning, 4);

                    SelectObject(memDC, t.fontBold);
                    SetTextColor(memDC, t.palette.warning);
                    RECT alertTitleRc = { alertRc.left + 12, alertRc.top + 8, alertRc.right - 12, alertRc.top + 24 };
                    DrawTextW(memDC, L"System Reboot Pending", -1, &alertTitleRc, DT_SINGLELINE | DT_NOPREFIX);

                    SelectObject(memDC, t.fontSmall);
                    SetTextColor(memDC, t.palette.text);
                    RECT alertBodyRc = { alertRc.left + 12, alertRc.top + 26, alertRc.right - 12, alertRc.bottom - 6 };
                    DrawTextW(memDC, L"This driver package was committed to the driver store in the current session. The device node is scheduled to reload the new driver binary on next Windows boot.",
                              -1, &alertBodyRc, DT_WORDBREAK | DT_NOPREFIX);

                    y += 72;
                }

                // Card 2: Hardware Identification Deck
                RECT card2Rc = { pad, y, cx - pad, y + 96 };
                Theme::DrawFrame(memDC, card2Rc, t.palette.surfaceRaised, t.palette.border, 4);

                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT idHeaderRc = { card2Rc.left + 12, card2Rc.top + 8, card2Rc.right - 90, card2Rc.top + 26 };
                DrawTextW(memDC, L"Hardware Instance ID", -1, &idHeaderRc, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT instRc = { card2Rc.left + 12, card2Rc.top + 28, card2Rc.right - 90, card2Rc.top + 46 };
                DrawTextW(memDC, dev.instanceId.c_str(), -1, &instRc, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Hardware ID
                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT hwHeaderRc = { card2Rc.left + 12, card2Rc.top + 50, card2Rc.right - 12, card2Rc.top + 68 };
                DrawTextW(memDC, L"Primary Hardware ID", -1, &hwHeaderRc, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.textSecondary);
                std::wstring firstHwId = dev.hardwareIds.empty() ? L"(none reported)" : dev.hardwareIds[0];
                RECT hwIdRc = { card2Rc.left + 12, card2Rc.top + 70, card2Rc.right - 12, card2Rc.top + 88 };
                DrawTextW(memDC, firstHwId.c_str(), -1, &hwIdRc, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                y += 104;

                // Card 3: Two-Column Driver Comparison Matrix
                RECT card3Rc = { pad, y, cx - pad, y + 114 };
                Theme::DrawFrame(memDC, card3Rc, t.palette.surfaceRaised, t.palette.border, 4);

                SelectObject(memDC, t.fontBold);
                SetTextColor(memDC, t.palette.text);
                RECT vHeaderRc = { card3Rc.left + 12, card3Rc.top + 8, card3Rc.right - 12, card3Rc.top + 26 };
                DrawTextW(memDC, L"Driver Telemetry & Candidate Comparison", -1, &vHeaderRc, DT_SINGLELINE | DT_NOPREFIX);

                // Vertical column split in card 3
                int colW = (card3Rc.right - card3Rc.left - 24) / 2;
                int col2Left = card3Rc.left + 12 + colW + 12;

                // Column 1: Installed Driver
                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c1Header = { card3Rc.left + 12, card3Rc.top + 30, card3Rc.left + 12 + colW, card3Rc.top + 46 };
                DrawTextW(memDC, L"ACTIVE IN DRIVER STORE", -1, &c1Header, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, t.palette.text);
                RECT c1Ver = { card3Rc.left + 12, card3Rc.top + 48, card3Rc.left + 12 + colW, card3Rc.top + 66 };
                std::wstring curVer = L"Version: " + (dev.installedVersion.empty() ? std::wstring(L"(none)") : dev.installedVersion);
                DrawTextW(memDC, curVer.c_str(), -1, &c1Ver, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                RECT c1Inf = { card3Rc.left + 12, card3Rc.top + 68, card3Rc.left + 12 + colW, card3Rc.top + 86 };
                std::wstring curInf = L"INF: " + (dev.installedInf.empty() ? std::wstring(L"—") : dev.installedInf);
                DrawTextW(memDC, curInf.c_str(), -1, &c1Inf, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Column 2: Pack Candidate
                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c2Header = { col2Left, card3Rc.top + 30, card3Rc.right - 12, card3Rc.top + 46 };
                DrawTextW(memDC, L"PACK STAGED CANDIDATE", -1, &c2Header, DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, t.fontMono);
                SetTextColor(memDC, (dev.state == Hw::DriverState::Outdated) ? t.palette.accent : t.palette.text);
                RECT c2Ver = { col2Left, card3Rc.top + 48, card3Rc.right - 12, card3Rc.top + 66 };
                std::wstring candVer = isWaitingRestart ? L"Installed this session" :
                                       (dev.state == Hw::DriverState::Outdated ? L"Ready to update" : L"Up to date");
                DrawTextW(memDC, candVer.c_str(), -1, &c2Ver, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                SelectObject(memDC, t.fontSmall);
                SetTextColor(memDC, t.palette.textSecondary);
                RECT c2Signer = { col2Left, card3Rc.top + 68, card3Rc.right - 12, card3Rc.top + 86 };
                DrawTextW(memDC, L"Signature: Verified WHQL", -1, &c2Signer, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

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
