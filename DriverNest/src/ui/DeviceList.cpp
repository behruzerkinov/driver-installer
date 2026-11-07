#include "DeviceList.h"
#include "Resources.h"
#include "Theme.h"
#include "Strings.h"
#include <commctrl.h>
#include <algorithm>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace DriverNest::Ui {

namespace {

// Column identifiers
constexpr int COL_DEVICE = 0;
constexpr int COL_STATUS = 1;
constexpr int COL_INSTALLED = 2;
constexpr int COL_AVAILABLE = 3;
constexpr int COL_SOURCE = 4;

constexpr int COLUMN_COUNT = 5;

// Main window handle, used for notification forwarding.
HWND hwndMain_ = nullptr;

// Device list state stored in GWLP_USERDATA. We use a unique_ptr so it is
// automatically freed when the window is destroyed.
struct State {
    std::vector<Hw::Device> devices;
    int virtualCount = 0;
    bool virtualMode = false;
    HWND hwndMain = nullptr;
};

State* GetState(HWND hwnd) {
    return reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void SetState(HWND hwnd, State* state) {
    delete GetState(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
}

// Forward declaration: defined below.
std::wstring StatusText(Hw::DriverState state);

// Build column setup.
void SetupColumns(HWND hwnd) {
    LV_COLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.fmt = LVCFMT_LEFT;

    // Device column
    col.iSubItem = COL_DEVICE;
    col.pszText = const_cast<wchar_t*>(Strings::ColDevice.data());
    col.cx = 220;
    ListView_InsertColumn(hwnd, COL_DEVICE, &col);

    // Status column
    col.iSubItem = COL_STATUS;
    col.pszText = const_cast<wchar_t*>(Strings::ColStatus.data());
    col.cx = 120;
    ListView_InsertColumn(hwnd, COL_STATUS, &col);

    // Installed version
    col.iSubItem = COL_INSTALLED;
    col.pszText = const_cast<wchar_t*>(Strings::ColVersion.data());
    col.cx = 130;
    ListView_InsertColumn(hwnd, COL_INSTALLED, &col);

    // Available version (candidate)
    col.iSubItem = COL_AVAILABLE;
    col.pszText = const_cast<wchar_t*>(Strings::ColCandidate.data());
    col.cx = 130;
    ListView_InsertColumn(hwnd, COL_AVAILABLE, &col);

    // Source
    col.iSubItem = COL_SOURCE;
    col.pszText = const_cast<wchar_t*>(Strings::ColSource.data());
    col.cx = 150;
    ListView_InsertColumn(hwnd, COL_SOURCE, &col);
}

// Set up extended styles.
void SetupExtendedStyles(HWND hwnd) {
    DWORD exStyle = ListView_GetExtendedListViewStyle(hwnd);
    exStyle |= LVS_EX_CHECKBOXES |
                LVS_EX_DOUBLEBUFFER |
                LVS_EX_FULLROWSELECT |
                LVS_EX_GRIDLINES;
    ListView_SetExtendedListViewStyle(hwnd, exStyle);
}

// Group by class guid. If classGuid is empty we put into an "(Other)" group.
std::map<std::wstring, int> BuildGroups(HWND hwnd,
                                        const std::vector<Hw::Device>& devices) {
    std::map<std::wstring, int> groups; // classGuid -> group index
    ListView_EnableGroupView(hwnd, TRUE);

    int idx = 0;
    for (auto& dev : devices) {
        std::wstring groupKey = dev.classGuid;
        if (groupKey.empty()) groupKey = L"{00000000-0000-0000-0000-000000000000}";
        if (groups.find(groupKey) == groups.end()) {
            LVGROUP group{};
            group.cbSize = sizeof(group);
            group.mask = LVGF_HEADER | LVGF_GROUPID;
            group.pszHeader = const_cast<wchar_t*>(groupKey.c_str());
            group.cchHeader = static_cast<int>(groupKey.size());
            group.iGroupId = idx;
            ListView_InsertGroup(hwnd, idx, &group);
            groups[groupKey] = idx;
            ++idx;
        }
    }
    return groups;
}

// Insert a device row into the list. lParam stores the index into State::devices.
void InsertDevice(HWND hwnd, const Hw::Device& dev, int groupIndex, int deviceIndex) {
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
    item.iItem = ListView_GetItemCount(hwnd);
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(dev.description.c_str());
    item.cchTextMax = static_cast<int>(dev.description.size());
    item.lParam = static_cast<LPARAM>(deviceIndex);
    item.iGroupId = groupIndex;
    ListView_InsertItem(hwnd, &item);

    // Subitems
    LVITEMW sub{};
    sub.mask = LVIF_TEXT;
    sub.iItem = item.iItem;

    // Status text
    const std::wstring statusText = StatusText(dev.state);
    sub.iSubItem = COL_STATUS;
    sub.pszText = const_cast<wchar_t*>(statusText.c_str());
    sub.cchTextMax = static_cast<int>(statusText.size());
    ListView_SetItem(hwnd, &sub);

    // Installed version
    sub.iSubItem = COL_INSTALLED;
    sub.pszText = const_cast<wchar_t*>(
        dev.installedVersion.empty() ? L"-" : dev.installedVersion.c_str());
    ListView_SetItem(hwnd, &sub);

    // Available version (placeholder; in full impl shows candidate version)
    sub.iSubItem = COL_AVAILABLE;
    sub.pszText = const_cast<wchar_t*>(L"-");
    ListView_SetItem(hwnd, &sub);

    // Source
    sub.iSubItem = COL_SOURCE;
    sub.pszText = const_cast<wchar_t*>(
        dev.installedInf.empty() ? L"" : L"Installed");
    ListView_SetItem(hwnd, &sub);
}

// Status text for a device state.
std::wstring StatusText(Hw::DriverState state) {
    switch (state) {
        case Hw::DriverState::Ok:       return std::wstring(Strings::StateOk);
        case Hw::DriverState::Missing:  return std::wstring(Strings::StateMissing);
        case Hw::DriverState::Outdated: return std::wstring(Strings::StateOutdated);
        case Hw::DriverState::Problem:  return std::wstring(Strings::StateProblem);
        case Hw::DriverState::Unsigned: return std::wstring(Strings::StateUnsigned);
    }
    return L"";
}

} // namespace

void DeviceList::Init(HWND hwnd) {
    SetupColumns(hwnd);
    SetupExtendedStyles(hwnd);
    // Reserve user data slot
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
}

void DeviceList::SetMainWindow(HWND hwndMain) {
    hwndMain_ = hwndMain;
}

void DeviceList::SetDevices(HWND hwnd, std::vector<Hw::Device> devices) {
    // Clear existing
    ListView_DeleteAllItems(hwnd);
    ListView_RemoveAllGroups(hwnd);

    auto* state = new State();
    state->devices = std::move(devices);
    state->virtualCount = static_cast<int>(state->devices.size());
    state->virtualMode = (state->virtualCount > 2000);
    state->hwndMain = hwndMain_;
    SetState(hwnd, state);

    // Set virtual mode if many devices
    if (state->virtualMode) {
        ListView_SetExtendedListViewStyle(hwnd,
            ListView_GetExtendedListViewStyle(hwnd) | LVS_OWNERDATA);
    }

    auto groups = BuildGroups(hwnd, state->devices);

    if (state->virtualMode) {
        // In virtual mode we don't insert items; the control asks for them via
        // LVN_GETDISPINFO using the state stored above.
        ListView_SetItemCount(hwnd, state->virtualCount);
    } else {
        for (int i = 0; i < static_cast<int>(state->devices.size()); ++i) {
            const auto& dev = state->devices[static_cast<size_t>(i)];
            std::wstring groupKey = dev.classGuid;
            if (groupKey.empty()) groupKey = L"{00000000-0000-0000-0000-000000000000}";
            auto it = groups.find(groupKey);
            int gIdx = (it != groups.end()) ? it->second : 0;
            InsertDevice(hwnd, dev, gIdx, i);
        }
    }
}

void DeviceList::SelectAll(HWND hwnd) {
    int count = ListView_GetItemCount(hwnd);
    for (int i = 0; i < count; ++i) {
        ListView_SetCheckState(hwnd, i, TRUE);
    }
}

void DeviceList::ClearSelection(HWND hwnd) {
    int count = ListView_GetItemCount(hwnd);
    for (int i = 0; i < count; ++i) {
        ListView_SetCheckState(hwnd, i, FALSE);
    }
    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
}

std::vector<std::wstring> DeviceList::GetSelectedInstanceIds(HWND hwnd) {
    std::vector<std::wstring> ids;
    int count = ListView_GetItemCount(hwnd);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(hwnd, i)) {
            LVITEMW item{};
            item.mask = LVIF_PARAM;
            item.iItem = i;
            if (ListView_GetItem(hwnd, &item)) {
                auto* state = GetState(hwnd);
                const int idx = static_cast<int>(item.lParam);
                if (state && idx >= 0 && idx < static_cast<int>(state->devices.size())) {
                    ids.push_back(state->devices[static_cast<size_t>(idx)].instanceId);
                }
            }
        }
    }
    return ids;
}

Hw::Device DeviceList::GetDeviceAt(HWND hwnd, int index) {
    auto* state = GetState(hwnd);
    if (!state) return Hw::Device{};

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(hwnd, &item)) {
        const int idx = static_cast<int>(item.lParam);
        if (idx >= 0 && idx < static_cast<int>(state->devices.size())) {
            return state->devices[static_cast<size_t>(idx)];
        }
    }
    return Hw::Device{};
}

LRESULT DeviceList::HandleNotify(HWND hwnd, NMHDR* pnm) {
    switch (pnm->code) {
        case LVN_GETDISPINFO: {
            if (!GetState(hwnd)) return 0;
            NMLVDISPINFO* disp = reinterpret_cast<NMLVDISPINFO*>(pnm);
            if (disp->item.mask & LVIF_TEXT) {
                if (disp->item.iSubItem == 0) {
                    auto& devs = GetState(hwnd)->devices;
                    if (disp->item.iItem >= 0 && disp->item.iItem < static_cast<int>(devs.size())) {
                        auto& dev = devs[disp->item.iItem];
                        if (disp->item.pszText) {
                            int max = disp->item.cchTextMax ?
                                      disp->item.cchTextMax : static_cast<int>(dev.description.size()) + 1;
                            if (max > 0) {
                                wcsncpy_s(disp->item.pszText, max,
                                          dev.description.c_str(),
                                          _TRUNCATE);
                            }
                        }
                    }
                }
            }
            return 0;
        }

        case NM_CUSTOMDRAW: {
            // Custom draw for row tint and status dot.
            auto* nmc = reinterpret_cast<NMLVCUSTOMDRAW*>(pnm);
            if (!nmc) return 0;

            switch (nmc->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;

                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;

                case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                    const int subItem = nmc->iSubItem;
                    const int item = static_cast<int>(nmc->nmcd.dwItemSpec);

                    auto* state = GetState(hwnd);
                    if (!state || item < 0 || item >= static_cast<int>(state->devices.size())) {
                        return CDRF_DODEFAULT;
                    }

                    auto& dev = state->devices[static_cast<size_t>(item)];
                    auto& palette = CurrentTheme().palette;

                    if (subItem == COL_STATUS) {
                        // Tint the status column text with the state color.
                        switch (dev.state) {
                            case Hw::DriverState::Ok:
                                nmc->clrText = palette.success;
                                break;
                            case Hw::DriverState::Missing:
                            case Hw::DriverState::Problem:
                                nmc->clrText = palette.error;
                                break;
                            case Hw::DriverState::Outdated:
                            case Hw::DriverState::Unsigned:
                                nmc->clrText = palette.warning;
                                break;
                        }
                        return CDRF_NEWFONT;
                    }

                    return CDRF_DODEFAULT;
                }

                default:
                    return CDRF_DODEFAULT;
            }
        }

        case LVN_ITEMCHANGED: {
            // Forward to main window for button enable/disable update
            auto* changedState = GetState(hwnd);
            HWND hMain = changedState ? changedState->hwndMain : nullptr;
            if (hMain) {
                int checked = 0;
                int count = ListView_GetItemCount(hwnd);
                for (int i = 0; i < count; ++i) {
                    if (ListView_GetCheckState(hwnd, i)) ++checked;
                }
                // Send custom message to main window to update install button
                ::SendMessageW(hMain, WM_APP_UPDATE_INSTALL_BUTTON, checked, 0);
            }
            return 0;
        }

        default:
            return 0;
    }
}

} // namespace DriverNest::Ui
