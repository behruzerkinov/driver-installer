#include "DeviceList.h"
#include "Resources.h"
#include "Theme.h"
#include "Strings.h"
#include "install/InstallHistory.h"
#include <commctrl.h>
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <vector>
#include <string>

namespace DriverNest::Ui {

namespace {

constexpr int COL_DEVICE    = 0;
constexpr int COL_STATUS    = 1;
constexpr int kRowHeight = 62;

HWND hwndMain_ = nullptr;

struct State {
    std::vector<Hw::Device> devices;
    std::vector<ItemProgress> progressList;
    std::vector<bool> checkedList;
    bool scanning = false;
    float shimmerPhase = 0.0f;
    int hoveredItem = -1;
    HWND hwndMain = nullptr;
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

void SetupColumns(HWND hwnd) {
    LV_COLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

    col.fmt = LVCFMT_LEFT;
    col.iSubItem = COL_DEVICE;
    col.pszText = const_cast<wchar_t*>(L"Device");
    col.cx = 320;
    ListView_InsertColumn(hwnd, COL_DEVICE, &col);

    col.fmt = LVCFMT_CENTER;
    col.iSubItem = COL_STATUS;
    col.pszText = const_cast<wchar_t*>(L"Driver status");
    col.cx = 170;
    ListView_InsertColumn(hwnd, COL_STATUS, &col);
}

void SetupExtendedStyles(HWND hwnd) {
    DWORD exStyle = ListView_GetExtendedListViewStyle(hwnd);
    exStyle |= LVS_EX_DOUBLEBUFFER |
               LVS_EX_FULLROWSELECT |
               LVS_EX_SUBITEMIMAGES;
    ListView_SetExtendedListViewStyle(hwnd, exStyle);

    // Fixed row height keeps rows stable at every DPI.
    HIMAGELIST hImgList = ImageList_Create(1, kRowHeight, ILC_COLOR32, 1, 1);
    ListView_SetImageList(hwnd, hImgList, LVSIL_SMALL);
}

} // namespace

void DeviceList::Init(HWND hwnd) {
    SetupColumns(hwnd);
    SetupExtendedStyles(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    RefreshTheme(hwnd);
}

void DeviceList::Layout(HWND hwnd, int width) {
    if (!hwnd || width <= 0) return;
    const int statusWidth = std::clamp(width * 34 / 100, 148, 220);
    ListView_SetColumnWidth(hwnd, COL_DEVICE, width - statusWidth);
    ListView_SetColumnWidth(hwnd, COL_STATUS, statusWidth);
}

void DeviceList::RefreshTheme(HWND hwnd) {
    if (!hwnd) return;
    const auto& palette = CurrentTheme().palette;
    ListView_SetBkColor(hwnd, palette.background);
    ListView_SetTextBkColor(hwnd, palette.background);
    ListView_SetTextColor(hwnd, palette.text);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void DeviceList::SetMainWindow(HWND hwndMain) {
    hwndMain_ = hwndMain;
}

void DeviceList::SetDevices(HWND hwnd, std::vector<Hw::Device> devices) {
    ListView_DeleteAllItems(hwnd);

    auto* s = new State();
    s->devices = std::move(devices);
    s->progressList.resize(s->devices.size());
    s->checkedList.resize(s->devices.size(), false);
    s->hwndMain = hwndMain_;

    // Pre-check outdated and missing devices by default
    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (!s->devices[i].candidateInf.empty() &&
            s->devices[i].state != Hw::DriverState::Ok &&
            s->devices[i].state != Hw::DriverState::PendingRestart) {
            s->checkedList[i] = true;
        }
    }

    SetState(hwnd, s);

    for (int i = 0; i < static_cast<int>(s->devices.size()); ++i) {
        LV_ITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = COL_DEVICE;
        item.pszText = const_cast<wchar_t*>(s->devices[static_cast<size_t>(i)].description.c_str());
        ListView_InsertItem(hwnd, &item);

        ListView_SetItemText(hwnd, i, COL_STATUS, const_cast<wchar_t*>(L""));
    }

    if (s->hwndMain) {
        ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, GetSelectedCount(hwnd), 0);
    }
}

int DeviceList::SetOnlineCandidates(HWND hwnd, const std::vector<Net::OnlineDriverCandidate>& candidates) {
    auto* s = GetState(hwnd);
    if (!s) return 0;
    for (auto& dev : s->devices) {
        dev.onlineCandidateVersion.clear();
        dev.onlineCandidateVendor.clear();
        dev.onlineCandidateDate.clear();
        dev.onlineCandidateFamily.clear();
        dev.onlineCandidateUrl.clear();
    }
    int matched = 0;
    for (const auto& candidate : candidates) {
        for (auto& dev : s->devices) {
            if (dev.instanceId != candidate.instanceId) continue;
            dev.onlineCandidateVersion = candidate.version;
            dev.onlineCandidateVendor = candidate.vendor;
            dev.onlineCandidateDate = candidate.date;
            dev.onlineCandidateFamily = candidate.family;
            dev.onlineCandidateUrl = candidate.url;
            ++matched;
            break;
        }
    }
    InvalidateRect(hwnd, nullptr, TRUE);
    return matched;
}

void DeviceList::SetScanning(HWND hwnd, bool scanning, float shimmerPhase) {
    auto* s = GetState(hwnd);
    if (!s) {
        s = new State();
        s->hwndMain = hwndMain_;
        SetState(hwnd, s);
    }
    s->scanning = scanning;
    s->shimmerPhase = shimmerPhase;
    InvalidateRect(hwnd, nullptr, FALSE);
}

bool DeviceList::IsScanning(HWND hwnd) {
    auto* s = GetState(hwnd);
    return s && s->scanning;
}

void DeviceList::SelectAll(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return;
    for (size_t i = 0; i < s->checkedList.size(); ++i) {
        if (!s->devices[i].candidateInf.empty() &&
            s->devices[i].state != Hw::DriverState::Ok &&
            s->devices[i].state != Hw::DriverState::PendingRestart) {
            s->checkedList[i] = true;
        }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    if (s->hwndMain) {
        ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, GetSelectedCount(hwnd), 0);
    }
}

void DeviceList::DeselectAll(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return;
    std::fill(s->checkedList.begin(), s->checkedList.end(), false);
    InvalidateRect(hwnd, nullptr, FALSE);
    if (s->hwndMain) {
        ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, 0, 0);
    }
}

void DeviceList::ToggleSelectAll(HWND hwnd) {
    if (AreAllSelected(hwnd)) {
        DeselectAll(hwnd);
    } else {
        SelectAll(hwnd);
    }
}

bool DeviceList::AreAllSelected(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s || s->checkedList.empty()) return false;
    bool hasCandidate = false;
    for (size_t i = 0; i < s->checkedList.size(); ++i) {
        if (!s->devices[i].candidateInf.empty() &&
            s->devices[i].state != Hw::DriverState::Ok &&
            s->devices[i].state != Hw::DriverState::PendingRestart) {
            hasCandidate = true;
            if (!s->checkedList[i]) return false;
        }
    }
    return hasCandidate;
}

int DeviceList::GetSelectedCount(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return 0;
    int count = 0;
    for (bool c : s->checkedList) {
        if (c) ++count;
    }
    return count;
}

void DeviceList::ClearSelection(HWND hwnd) {
    DeselectAll(hwnd);
}

std::vector<std::wstring> DeviceList::GetSelectedInstanceIds(HWND hwnd) {
    std::vector<std::wstring> result;
    auto* s = GetState(hwnd);
    if (!s) return result;

    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (i < s->checkedList.size() && s->checkedList[i]) {
            result.push_back(s->devices[i].instanceId);
        }
    }
    return result;
}

std::vector<Hw::Device> DeviceList::GetSelectedDevices(HWND hwnd) {
    std::vector<Hw::Device> result;
    auto* s = GetState(hwnd);
    if (!s) return result;
    result.reserve(s->devices.size());
    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (i < s->checkedList.size() && s->checkedList[i] &&
            !s->devices[i].candidateInf.empty() &&
            s->devices[i].state != Hw::DriverState::Ok &&
            s->devices[i].state != Hw::DriverState::PendingRestart) {
            result.push_back(s->devices[i]);
        }
    }
    return result;
}

bool DeviceList::SelectOnlyForInstall(HWND hwnd, std::wstring_view instanceId) {
    auto* s = GetState(hwnd);
    if (!s) return false;
    size_t target = s->devices.size();
    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (s->devices[i].instanceId == instanceId &&
            !s->devices[i].candidateInf.empty() &&
            s->devices[i].state != Hw::DriverState::PendingRestart) {
            target = i;
            break;
        }
    }
    if (target == s->devices.size()) return false;
    std::fill(s->checkedList.begin(), s->checkedList.end(), false);
    s->checkedList[target] = true;
    InvalidateRect(hwnd, nullptr, FALSE);
    if (s->hwndMain) {
        ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, 1, 0);
    }
    return true;
}

Hw::Device DeviceList::GetDeviceAt(HWND hwnd, int index) {
    auto* s = GetState(hwnd);
    if (!s || index < 0 || index >= static_cast<int>(s->devices.size())) {
        return Hw::Device{};
    }
    return s->devices[static_cast<size_t>(index)];
}

int DeviceList::GetDeviceCount(HWND hwnd) {
    auto* s = GetState(hwnd);
    return s ? static_cast<int>(s->devices.size()) : 0;
}

int DeviceList::FindMatchingDevice(HWND hwnd, std::wstring_view query) {
    auto* s = GetState(hwnd);
    if (!s || query.empty()) return -1;
    std::wstring needle(query);
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    auto contains = [&needle](std::wstring_view value) {
        std::wstring folded(value);
        std::transform(folded.begin(), folded.end(), folded.begin(),
                       [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return folded.find(needle) != std::wstring::npos;
    };
    for (size_t i = 0; i < s->devices.size(); ++i) {
        const auto& dev = s->devices[i];
        if (contains(dev.description) || contains(dev.instanceId) || contains(dev.classGuid) ||
            contains(dev.manufacturer)) return static_cast<int>(i);
        for (const auto& id : dev.hardwareIds) {
            if (contains(id)) return static_cast<int>(i);
        }
        for (const auto& id : dev.compatibleIds) {
            if (contains(id)) return static_cast<int>(i);
        }
    }
    return -1;
}

void DeviceList::SetItemProgress(HWND hwnd, int index, const ItemProgress& progress) {
    auto* s = GetState(hwnd);
    if (!s || index < 0 || index >= static_cast<int>(s->progressList.size())) return;

    s->progressList[static_cast<size_t>(index)] = progress;
    if (progress.state == ItemInstallState::PendingRestart) {
        auto& device = s->devices[static_cast<size_t>(index)];
        device.state = Hw::DriverState::PendingRestart;
        device.candidateInf.clear();
        device.candidateVersion.clear();
        device.candidateSigned = false;
        if (static_cast<size_t>(index) < s->checkedList.size()) {
            s->checkedList[static_cast<size_t>(index)] = false;
        }
        if (s->hwndMain) {
            ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, GetSelectedCount(hwnd), 0);
        }
    }
    RECT rc;
    if (ListView_GetItemRect(hwnd, index, &rc, LVIR_BOUNDS)) {
        InvalidateRect(hwnd, &rc, FALSE);
    }
}

void DeviceList::SetItemProgressByInstanceId(HWND hwnd, const std::wstring& instanceId, const ItemProgress& progress) {
    auto* s = GetState(hwnd);
    if (!s) return;

    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (s->devices[i].instanceId == instanceId) {
            SetItemProgress(hwnd, static_cast<int>(i), progress);
            break;
        }
    }
}

void DeviceList::ResetProgress(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return;
    for (auto& p : s->progressList) {
        p = ItemProgress{};
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

bool DeviceList::HandleClick(HWND hwnd, int x, int y) {
    LVHITTESTINFO hit{};
    hit.pt.x = x;
    hit.pt.y = y;
    int idx = ListView_HitTest(hwnd, &hit);
    if (idx >= 0) {
        auto* s = GetState(hwnd);
        if (!s || idx >= static_cast<int>(s->checkedList.size())) return false;

        RECT rc;
        if (ListView_GetSubItemRect(hwnd, idx, COL_DEVICE, LVIR_BOUNDS, &rc)) {
            // Click hit area on checkbox: x in [rc.left + 4, rc.left + 32]
            if (x >= rc.left + 4 && x <= rc.left + 32) {
                if (!s->devices[static_cast<size_t>(idx)].candidateInf.empty() &&
                    s->devices[static_cast<size_t>(idx)].state != Hw::DriverState::Ok &&
                    s->devices[static_cast<size_t>(idx)].state != Hw::DriverState::PendingRestart) {
                    s->checkedList[static_cast<size_t>(idx)] = !s->checkedList[static_cast<size_t>(idx)];
                    InvalidateRect(hwnd, &rc, FALSE);
                    if (s->hwndMain) {
                        ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, GetSelectedCount(hwnd), 0);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

LRESULT DeviceList::HandleNotify(HWND hwnd, NMHDR* pnm) {
    switch (pnm->code) {
        case NM_CLICK: {
            auto* pnmItem = reinterpret_cast<NMITEMACTIVATE*>(pnm);
            if (pnmItem && pnmItem->iItem >= 0) {
                if (HandleClick(hwnd, pnmItem->ptAction.x, pnmItem->ptAction.y)) {
                    return 1;
                }
            }
            return 0;
        }

        case NM_CUSTOMDRAW: {
            auto* nmc = reinterpret_cast<NMLVCUSTOMDRAW*>(pnm);
            if (!nmc) return CDRF_DODEFAULT;

            auto* s = GetState(hwnd);
            auto& t = CurrentTheme();

            switch (nmc->nmcd.dwDrawStage) {
                case CDDS_PREPAINT: {
                    if (s && s->scanning) {
                        HDC hdc = nmc->nmcd.hdc;
                        RECT clientRc;
                        GetClientRect(hwnd, &clientRc);

                        // Translucent glass fill over blurry Acrylic backdrop
                        Theme::DrawGlassCard(hdc, clientRc, t.palette.surface, 210, 0, 0, false);

                        int rows = std::min(7, static_cast<int>((clientRc.bottom - clientRc.top) / kRowHeight + 1));

                        for (int r = 0; r < rows; ++r) {
                            int y = clientRc.top + r * kRowHeight;
                            RECT rowRc = { clientRc.left + 6, y + 4, clientRc.right - 6, y + kRowHeight - 4 };

                            // Skeleton Card container
                            Theme::DrawGlassCard(hdc, rowRc, t.palette.surfaceRaised, 180, t.palette.glassBorderSubtle, 8, false);

                            // Checkbox skeleton
                            RECT cbRc = { rowRc.left + 10, y + 18, rowRc.left + 28, y + 36 };
                            Theme::DrawScanlineSkeleton(hdc, cbRc, s->shimmerPhase + r * 0.35f);

                            // Squircle avatar skeleton
                            RECT icRc = { rowRc.left + 36, y + 10, rowRc.left + 74, y + 48 };
                            Theme::DrawScanlineSkeleton(hdc, icRc, s->shimmerPhase + r * 0.35f + 0.1f);

                            // Title skeleton
                            int w = 180 + ((r * 43) % 100);
                            RECT titleRc = { rowRc.left + 84, y + 12, rowRc.left + 84 + w, y + 26 };
                            Theme::DrawScanlineSkeleton(hdc, titleRc, s->shimmerPhase + r * 0.35f + 0.2f);

                            // Subtitle skeleton
                            RECT subRc = { rowRc.left + 84, y + 32, rowRc.left + 220, y + 44 };
                            Theme::DrawScanlineSkeleton(hdc, subRc, s->shimmerPhase + r * 0.35f + 0.35f);

                            // Status Pill skeleton
                            RECT statusRc = { rowRc.right - 140, y + 16, rowRc.right - 12, y + 38 };
                            Theme::DrawScanlineSkeleton(hdc, statusRc, s->shimmerPhase + r * 0.35f + 0.5f);
                        }
                        return CDRF_SKIPDEFAULT;
                    }
                    return CDRF_NOTIFYITEMDRAW;
                }

                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;

                case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                    const int subItem = nmc->iSubItem;
                    const int item = static_cast<int>(nmc->nmcd.dwItemSpec);
                    HDC hdc = nmc->nmcd.hdc;

                    if (!s || item < 0 || item >= static_cast<int>(s->devices.size())) {
                        return CDRF_DODEFAULT;
                    }

                    const auto& dev = s->devices[static_cast<size_t>(item)];
                    const auto& prog = s->progressList[static_cast<size_t>(item)];
                    bool checked = s->checkedList[static_cast<size_t>(item)];
                    bool selected = (ListView_GetItemState(hwnd, item, LVIS_SELECTED) & LVIS_SELECTED) != 0;

                    RECT subRc;
                    ListView_GetSubItemRect(hwnd, item, subItem, LVIR_BOUNDS, &subRc);

                    // Row Card Container Rect
                    RECT cardRc = { subRc.left + 4, subRc.top + 2, subRc.right - 4, subRc.bottom - 2 };

                    if (subItem == COL_DEVICE) {
                        // Background Card (Unigram Chat Item Card)
                        if (selected) {
                            Theme::DrawGlassCard(hdc, cardRc, t.palette.selection, 240, t.palette.accent, 8, true);

                            // Left Accent Indicator Pill (Unigram signature)
                            RECT barRc = { cardRc.left + 2, cardRc.top + 8, cardRc.left + 5, cardRc.bottom - 8 };
                            HBRUSH barBrush = CreateSolidBrush(t.palette.accent);
                            FillRect(hdc, &barRc, barBrush);
                            DeleteObject(barBrush);
                        } else {
                            COLORREF cardBg = (item % 2 == 1) ? t.palette.surfaceRaised : t.palette.surface;
                            Theme::DrawGlassCard(hdc, cardRc, cardBg, 190, t.palette.glassBorderSubtle, 8, false);
                        }

                        // 1. Checkbox
                        int cbSize = 18;
                        int cbX = cardRc.left + 10;
                        int cbY = cardRc.top + (cardRc.bottom - cardRc.top - cbSize) / 2;
                        bool disabled = (dev.state == Hw::DriverState::PendingRestart);
                        Theme::DrawModernCheckbox(hdc, cbX, cbY, cbSize, checked, false, disabled);

                        // 2. Unigram Squircle Category Avatar
                        RECT sqRc = { cbX + cbSize + 8, cardRc.top + 7, cbX + cbSize + 8 + 38, cardRc.top + 7 + 38 };
                        COLORREF catCol = GetClassAccent(dev.classGuid, t.palette);
                        Theme::DrawSquircleAvatar(hdc, sqRc, dev.classGuid, catCol, 10);

                        // 3. Two-Line Typography
                        int textX = sqRc.right + 10;
                        int textW = cardRc.right - textX - 10;

                        // Line 1: Primary Description
                        SetBkMode(hdc, TRANSPARENT);
                        HGDIOBJ oldFont = SelectObject(hdc, t.fontBold);
                        SetTextColor(hdc, selected ? RGB(255, 255, 255) : t.palette.text);

                        RECT titleRc = { textX, cardRc.top + 8, textX + textW, cardRc.top + 26 };
                        DrawTextW(hdc, dev.description.c_str(), -1, &titleRc,
                                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                        // Line 2: Monospace Telemetry & Version Transition
                        SelectObject(hdc, t.fontMono);
                        SetTextColor(hdc, selected ? RGB(220, 235, 255) : t.palette.textSecondary);

                        std::wstring subStr = dev.installedVersion.empty()
                            ? L"No driver installed"
                            : (L"Installed " + dev.installedVersion);
                        if (!dev.candidateVersion.empty()) {
                            subStr += L"    →    Candidate " + dev.candidateVersion;
                        } else if (!dev.onlineCandidateVersion.empty()) {
                            subStr += L"    •    Online " + dev.onlineCandidateVersion + L" via " + dev.onlineCandidateVendor;
                        }
                        RECT subLineRc = { textX, cardRc.top + 28, textX + textW, cardRc.top + 46 };
                        DrawTextW(hdc, subStr.c_str(), -1, &subLineRc,
                                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

                        // Inline Mini Progress Bar when installing
                        if (prog.state == ItemInstallState::Downloading) {
                            RECT progRc = { textX, cardRc.bottom - 6, cardRc.right - 14, cardRc.bottom - 3 };
                            Theme::DrawTelemetryProgressBar(hdc, progRc, prog.percent, t.palette.accent, t.palette.surfaceHover);
                        }

                        SelectObject(hdc, oldFont);
                        return CDRF_SKIPDEFAULT;
                    }

                    if (subItem == COL_STATUS) {
                        RECT pillRc = cardRc;
                        pillRc.left += 8;
                        pillRc.right -= 8;
                        pillRc.top += 12;
                        pillRc.bottom -= 12;

                        if (prog.state == ItemInstallState::Downloading) {
                            Theme::DrawTelemetryProgressBar(hdc, pillRc, prog.percent, t.palette.accent, t.palette.surfaceRaised);
                            std::wstring pctStr = std::to_wstring(prog.percent) + L"% (" +
                                                  std::to_wstring(prog.bytesCurrent / (1024 * 1024)) + L" MB)";
                            HGDIOBJ oldFont = SelectObject(hdc, t.fontMono);
                            SetTextColor(hdc, t.palette.text);
                            DrawTextW(hdc, pctStr.c_str(), -1, &pillRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                            SelectObject(hdc, oldFont);
                        } else if (prog.state == ItemInstallState::Queued) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"In Install Queue", t.palette.surfaceRaised, t.palette.textSecondary, t.palette.border, 0);
                        } else if (prog.state == ItemInstallState::Installing) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Staging INF...", t.palette.accentLight, t.palette.accent, t.palette.accent, 1);
                        } else if (prog.state == ItemInstallState::Verifying) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Verifying Store", t.palette.infoBg, t.palette.info, t.palette.info, 1);
                        } else if (prog.state == ItemInstallState::Installed) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Installed (Active)", t.palette.successBg, t.palette.success, t.palette.success, 1);
                        } else if (prog.state == ItemInstallState::PendingRestart) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Reboot Required", t.palette.warningBg, t.palette.warning, t.palette.warning, 1);
                        } else if (prog.state == ItemInstallState::Failed) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Install Failed", t.palette.errorBg, t.palette.error, t.palette.error, 1);
                        } else if (!dev.onlineCandidateUrl.empty() && dev.candidateInf.empty()) {
                            Theme::DrawUnigramPill(hdc, pillRc, L"Online download", t.palette.infoBg, t.palette.info, t.palette.info, 1);
                        } else {
                            switch (dev.state) {
                                case Hw::DriverState::PendingRestart:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Reboot Pending", t.palette.warningBg, t.palette.warning, t.palette.warning, 1);
                                    break;
                                case Hw::DriverState::Ok:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Up to Date", t.palette.successBg, t.palette.success, t.palette.success, 1);
                                    break;
                                case Hw::DriverState::Missing:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Missing Driver", t.palette.warningBg, t.palette.warning, t.palette.warning, 1);
                                    break;
                                case Hw::DriverState::Outdated:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Update Ready", t.palette.accentLight, t.palette.accent, t.palette.accent, 1);
                                    break;
                                case Hw::DriverState::Problem:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Hardware Code 28", t.palette.errorBg, t.palette.error, t.palette.error, 1);
                                    break;
                                case Hw::DriverState::Unsigned:
                                    Theme::DrawUnigramPill(hdc, pillRc, L"Unsigned INF", t.palette.surfaceRaised, t.palette.textSecondary, t.palette.border, 0);
                                    break;
                            }
                        }
                        return CDRF_SKIPDEFAULT;
                    }

                    return CDRF_DODEFAULT;
                }

                default:
                    return CDRF_DODEFAULT;
            }
        }

        default:
            return 0;
    }
}

} // namespace DriverNest::Ui
