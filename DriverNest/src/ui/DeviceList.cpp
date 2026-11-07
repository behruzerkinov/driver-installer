#include "DeviceList.h"
#include "Resources.h"
#include "Theme.h"
#include "Strings.h"
#include "install/InstallHistory.h"
#include <commctrl.h>
#include <algorithm>
#include <cwchar>
#include <vector>
#include <string>

namespace DriverNest::Ui {

namespace {

constexpr int COL_DEVICE    = 0;
constexpr int COL_STATUS    = 1;
constexpr int COL_INSTALLED = 2;
constexpr int COL_AVAILABLE = 3;
constexpr int COL_SOURCE    = 4;

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

void SetupColumns(HWND hwnd) {
    LV_COLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

    col.fmt = LVCFMT_LEFT;
    col.iSubItem = COL_DEVICE;
    col.pszText = const_cast<wchar_t*>(L"Hardware Node / Device");
    col.cx = 290;
    ListView_InsertColumn(hwnd, COL_DEVICE, &col);

    col.fmt = LVCFMT_CENTER;
    col.iSubItem = COL_STATUS;
    col.pszText = const_cast<wchar_t*>(L"Driver State");
    col.cx = 170;
    ListView_InsertColumn(hwnd, COL_STATUS, &col);

    col.fmt = LVCFMT_LEFT;
    col.iSubItem = COL_INSTALLED;
    col.pszText = const_cast<wchar_t*>(L"Installed");
    col.cx = 135;
    ListView_InsertColumn(hwnd, COL_INSTALLED, &col);

    col.fmt = LVCFMT_LEFT;
    col.iSubItem = COL_AVAILABLE;
    col.pszText = const_cast<wchar_t*>(L"Pack Staged");
    col.cx = 135;
    ListView_InsertColumn(hwnd, COL_AVAILABLE, &col);

    col.fmt = LVCFMT_LEFT;
    col.iSubItem = COL_SOURCE;
    col.pszText = const_cast<wchar_t*>(L"Repository");
    col.cx = 120;
    ListView_InsertColumn(hwnd, COL_SOURCE, &col);
}

void SetupExtendedStyles(HWND hwnd) {
    DWORD exStyle = ListView_GetExtendedListViewStyle(hwnd);
    exStyle |= LVS_EX_DOUBLEBUFFER |
               LVS_EX_FULLROWSELECT |
               LVS_EX_SUBITEMIMAGES;
    ListView_SetExtendedListViewStyle(hwnd, exStyle);

    // Enforce 38px row height via imagelist
    HIMAGELIST hImgList = ImageList_Create(1, 38, ILC_COLOR32, 1, 1);
    ListView_SetImageList(hwnd, hImgList, LVSIL_SMALL);
}

} // namespace

void DeviceList::Init(HWND hwnd) {
    SetupColumns(hwnd);
    SetupExtendedStyles(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
}

void DeviceList::SetMainWindow(HWND hwndMain) {
    hwndMain_ = hwndMain;
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
    return s ? s->scanning : false;
}

void DeviceList::SetDevices(HWND hwnd, std::vector<Hw::Device> devices) {
    ListView_DeleteAllItems(hwnd);

    auto* state = new State();
    state->devices = std::move(devices);
    state->hwndMain = hwndMain_;
    state->scanning = false;
    state->progressList.resize(state->devices.size());
    state->checkedList.resize(state->devices.size(), false);

    // Pre-select out-of-date or missing drivers (never select reboot-pending)
    for (size_t i = 0; i < state->devices.size(); ++i) {
        auto& d = state->devices[i];
        if (d.state == Hw::DriverState::PendingRestart) {
            state->checkedList[i] = false;
            state->progressList[i].state = ItemInstallState::PendingRestart;
        } else if (d.state == Hw::DriverState::Outdated || d.state == Hw::DriverState::Missing) {
            state->checkedList[i] = true;
        } else {
            state->checkedList[i] = false;
        }
    }

    SetState(hwnd, state);

    for (int i = 0; i < static_cast<int>(state->devices.size()); ++i) {
        const auto& dev = state->devices[static_cast<size_t>(i)];

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = COL_DEVICE;
        item.pszText = const_cast<wchar_t*>(dev.description.c_str());
        item.lParam = static_cast<LPARAM>(i);
        ListView_InsertItem(hwnd, &item);

        ListView_SetItemText(hwnd, i, COL_STATUS, const_cast<wchar_t*>(L""));
        ListView_SetItemText(hwnd, i, COL_INSTALLED,
                             const_cast<wchar_t*>(dev.installedVersion.empty() ? L"-" : dev.installedVersion.c_str()));
        ListView_SetItemText(hwnd, i, COL_AVAILABLE, const_cast<wchar_t*>(L"-"));
        ListView_SetItemText(hwnd, i, COL_SOURCE,
                             const_cast<wchar_t*>(dev.installedInf.empty() ? L"Pack Repository" : L"System"));
    }

    if (hwndMain_) {
        int checked = GetSelectedCount(hwnd);
        ::PostMessageW(hwndMain_, WM_APP_UPDATE_INSTALL_BUTTON, checked, 0);
    }
}

void DeviceList::SelectAll(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return;
    for (size_t i = 0; i < s->checkedList.size(); ++i) {
        if (s->devices[i].state != Hw::DriverState::PendingRestart) {
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
    for (size_t i = 0; i < s->checkedList.size(); ++i) {
        s->checkedList[i] = false;
    }
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
    if (!s || s->devices.empty()) return false;
    int eligible = 0;
    int checkedEligible = 0;
    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (s->devices[i].state != Hw::DriverState::PendingRestart &&
            s->devices[i].state != Hw::DriverState::Ok) {
            ++eligible;
            if (s->checkedList[i]) ++checkedEligible;
        }
    }
    if (eligible == 0) {
        return GetSelectedCount(hwnd) == static_cast<int>(s->devices.size());
    }
    return checkedEligible >= eligible;
}

int DeviceList::GetSelectedCount(HWND hwnd) {
    auto* s = GetState(hwnd);
    if (!s) return 0;
    int count = 0;
    for (bool chk : s->checkedList) {
        if (chk) ++count;
    }
    return count;
}

void DeviceList::ClearSelection(HWND hwnd) {
    DeselectAll(hwnd);
    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
}

std::vector<std::wstring> DeviceList::GetSelectedInstanceIds(HWND hwnd) {
    std::vector<std::wstring> ids;
    auto* s = GetState(hwnd);
    if (!s) return ids;

    for (size_t i = 0; i < s->devices.size(); ++i) {
        if (i < s->checkedList.size() && s->checkedList[i]) {
            ids.push_back(s->devices[i].instanceId);
        }
    }
    return ids;
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

void DeviceList::SetItemProgress(HWND hwnd, int index, const ItemProgress& progress) {
    auto* s = GetState(hwnd);
    if (!s || index < 0 || index >= static_cast<int>(s->progressList.size())) return;

    s->progressList[static_cast<size_t>(index)] = progress;
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

bool DeviceList::HandleClick(HWND hwnd, int x, int /*y*/) {
    LVHITTESTINFO hit{};
    hit.pt.x = x;
    hit.pt.y = 0;
    int idx = ListView_HitTest(hwnd, &hit);
    if (idx >= 0) {
        auto* s = GetState(hwnd);
        if (!s || idx >= static_cast<int>(s->checkedList.size())) return false;

        RECT rc;
        if (ListView_GetSubItemRect(hwnd, idx, COL_DEVICE, LVIR_BOUNDS, &rc)) {
            if (x >= rc.left + 6 && x <= rc.left + 36) {
                s->checkedList[static_cast<size_t>(idx)] = !s->checkedList[static_cast<size_t>(idx)];
                InvalidateRect(hwnd, &rc, FALSE);
                if (s->hwndMain) {
                    ::PostMessageW(s->hwndMain, WM_APP_UPDATE_INSTALL_BUTTON, GetSelectedCount(hwnd), 0);
                }
                return true;
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
                        FillRect(hdc, &clientRc, t.brushSurface);

                        int rowH = 38;
                        int rows = std::min(8, static_cast<int>((clientRc.bottom - clientRc.top) / rowH + 1));

                        for (int r = 0; r < rows; ++r) {
                            int y = clientRc.top + r * rowH;
                            RECT rowRc = { clientRc.left, y, clientRc.right, y + rowH };

                            HPEN linePen = CreatePen(PS_SOLID, 1, t.palette.border);
                            HGDIOBJ oldPen = SelectObject(hdc, linePen);
                            MoveToEx(hdc, rowRc.left, rowRc.bottom - 1, nullptr);
                            LineTo(hdc, rowRc.right, rowRc.bottom - 1);
                            SelectObject(hdc, oldPen);
                            DeleteObject(linePen);

                            // Skeleton Checkbox
                            RECT cbRc = { rowRc.left + 12, y + 11, rowRc.left + 28, y + 27 };
                            Theme::DrawScanlineSkeleton(hdc, cbRc, s->shimmerPhase + r * 0.35f);

                            // Skeleton Category Icon
                            RECT icRc = { rowRc.left + 36, y + 12, rowRc.left + 50, y + 26 };
                            Theme::DrawScanlineSkeleton(hdc, icRc, s->shimmerPhase + r * 0.35f + 0.1f);

                            // Skeleton Title Bar
                            int w = 170 + ((r * 43) % 95);
                            RECT titleRc = { rowRc.left + 58, y + 12, rowRc.left + 58 + w, y + 26 };
                            Theme::DrawScanlineSkeleton(hdc, titleRc, s->shimmerPhase + r * 0.35f + 0.2f);

                            // Skeleton Status Pill
                            RECT statusRc = { rowRc.left + 310, y + 8, rowRc.left + 430, y + 30 };
                            Theme::DrawScanlineSkeleton(hdc, statusRc, s->shimmerPhase + r * 0.35f + 0.35f);

                            // Skeleton Version Bar
                            RECT verRc = { rowRc.left + 480, y + 12, rowRc.left + 580, y + 26 };
                            Theme::DrawScanlineSkeleton(hdc, verRc, s->shimmerPhase + r * 0.35f + 0.5f);
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

                    // Row Background
                    COLORREF bgCol = selected ? t.palette.selection
                                              : (item % 2 == 1 ? t.palette.surfaceRaised : t.palette.surface);
                    HBRUSH bgBrush = CreateSolidBrush(bgCol);
                    FillRect(hdc, &subRc, bgBrush);
                    DeleteObject(bgBrush);

                    // Subtle hairline divider
                    HPEN linePen = CreatePen(PS_SOLID, 1, t.palette.border);
                    HGDIOBJ oldPen = SelectObject(hdc, linePen);
                    MoveToEx(hdc, subRc.left, subRc.bottom - 1, nullptr);
                    LineTo(hdc, subRc.right, subRc.bottom - 1);
                    SelectObject(hdc, oldPen);
                    DeleteObject(linePen);

                    SetBkMode(hdc, TRANSPARENT);

                    if (subItem == COL_DEVICE) {
                        // 1. Checkbox
                        int cbSize = 16;
                        int cbX = subRc.left + 10;
                        int cbY = subRc.top + (subRc.bottom - subRc.top - cbSize) / 2;
                        bool disabled = (dev.state == Hw::DriverState::PendingRestart);
                        Theme::DrawModernCheckbox(hdc, cbX, cbY, cbSize, checked, false, disabled);

                        // 2. Hardware Category Emblem
                        int icX = cbX + cbSize + 10;
                        int icY = subRc.top + (subRc.bottom - subRc.top - 14) / 2;
                        COLORREF icColor = selected ? t.palette.selectionText : t.palette.textSecondary;
                        Theme::DrawCategoryIcon(hdc, icX, icY, dev.classGuid, icColor);

                        // 3. Hardware Name / Description
                        RECT textRc = subRc;
                        textRc.left = icX + 22;
                        textRc.right -= 6;

                        HGDIOBJ oldFont = SelectObject(hdc, t.fontRegular);
                        SetTextColor(hdc, selected ? t.palette.selectionText : t.palette.text);
                        DrawTextW(hdc, dev.description.c_str(), -1, &textRc,
                                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                        SelectObject(hdc, oldFont);
                        return CDRF_SKIPDEFAULT;
                    }

                    if (subItem == COL_STATUS) {
                        RECT pillRc = subRc;
                        pillRc.left += 10;
                        pillRc.right -= 10;
                        pillRc.top += 8;
                        pillRc.bottom -= 8;

                        if (prog.state == ItemInstallState::Downloading) {
                            Theme::DrawTelemetryProgressBar(hdc, pillRc, prog.percent, t.palette.accent, t.palette.surfaceRaised);
                            std::wstring pctStr = std::to_wstring(prog.percent) + L"% (" +
                                                  std::to_wstring(prog.bytesCurrent / (1024 * 1024)) + L" MB)";
                            HGDIOBJ oldFont = SelectObject(hdc, t.fontMono);
                            SetTextColor(hdc, t.palette.text);
                            DrawTextW(hdc, pctStr.c_str(), -1, &pillRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                            SelectObject(hdc, oldFont);
                        } else if (prog.state == ItemInstallState::Queued) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Staged in Queue", t.palette.surfaceRaised, t.palette.textSecondary, 0);
                        } else if (prog.state == ItemInstallState::Installing) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Installing Node...", t.palette.accentLight, t.palette.accent, 1);
                        } else if (prog.state == ItemInstallState::Verifying) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Verifying Store...", t.palette.infoBg, t.palette.info, 1);
                        } else if (prog.state == ItemInstallState::Installed) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Installed (Active)", t.palette.successBg, t.palette.success, 1);
                        } else if (prog.state == ItemInstallState::PendingRestart) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Reboot Pending", t.palette.warningBg, t.palette.warning, 1);
                        } else if (prog.state == ItemInstallState::Failed) {
                            Theme::DrawSignalBadge(hdc, pillRc, L"Install Fault", t.palette.errorBg, t.palette.error, 1);
                        } else {
                            switch (dev.state) {
                                case Hw::DriverState::PendingRestart:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Reboot Pending", t.palette.warningBg, t.palette.warning, 1);
                                    break;
                                case Hw::DriverState::Ok:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Verified Current", t.palette.successBg, t.palette.success, 1);
                                    break;
                                case Hw::DriverState::Missing:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Driver Missing", t.palette.warningBg, t.palette.warning, 1);
                                    break;
                                case Hw::DriverState::Outdated:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Update Available", t.palette.accentLight, t.palette.accent, 1);
                                    break;
                                case Hw::DriverState::Problem:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Hardware Fault", t.palette.errorBg, t.palette.error, 1);
                                    break;
                                case Hw::DriverState::Unsigned:
                                    Theme::DrawSignalBadge(hdc, pillRc, L"Unsigned Driver", t.palette.surfaceRaised, t.palette.textSecondary, 0);
                                    break;
                            }
                        }
                        return CDRF_SKIPDEFAULT;
                    }

                    if (subItem == COL_INSTALLED) {
                        RECT textRc = subRc;
                        textRc.left += 8;
                        textRc.right -= 8;
                        HGDIOBJ oldFont = SelectObject(hdc, t.fontMono);
                        SetTextColor(hdc, selected ? t.palette.selectionText : t.palette.textSecondary);
                        const wchar_t* txt = dev.installedVersion.empty() ? L"(none)" : dev.installedVersion.c_str();
                        DrawTextW(hdc, txt, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                        SelectObject(hdc, oldFont);
                        return CDRF_SKIPDEFAULT;
                    }

                    if (subItem == COL_AVAILABLE) {
                        RECT textRc = subRc;
                        textRc.left += 8;
                        textRc.right -= 8;
                        HGDIOBJ oldFont = SelectObject(hdc, t.fontMono);
                        if (dev.state == Hw::DriverState::Outdated || dev.state == Hw::DriverState::Missing) {
                            SetTextColor(hdc, t.palette.accent);
                            DrawTextW(hdc, L"Candidate Ready", -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                        } else {
                            SetTextColor(hdc, selected ? t.palette.selectionText : t.palette.textSecondary);
                            DrawTextW(hdc, L"—", -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                        }
                        SelectObject(hdc, oldFont);
                        return CDRF_SKIPDEFAULT;
                    }

                    if (subItem == COL_SOURCE) {
                        RECT textRc = subRc;
                        textRc.left += 8;
                        textRc.right -= 8;
                        HGDIOBJ oldFont = SelectObject(hdc, t.fontSmall);
                        SetTextColor(hdc, selected ? t.palette.selectionText : t.palette.textSecondary);
                        const wchar_t* txt = dev.installedInf.empty() ? L"Pack Repository" : L"Driver Store";
                        DrawTextW(hdc, txt, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                        SelectObject(hdc, oldFont);
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
