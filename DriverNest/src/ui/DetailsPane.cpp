#include "DetailsPane.h"
#include "Resources.h"
#include "DeviceList.h"
#include "Theme.h"
#include "Strings.h"
#include <commctrl.h>
#include <algorithm>
#include <memory>

namespace DriverNest::Ui {

namespace {

// Simple owner-drawn button that can show an icon + text.
// Not full implementation; uses standard button with BS_OWNERDRAW where needed.
// For v1 we use standard buttons with text labels.

// Create a label (static) control with a given text.
HWND CreateLabel(HWND hwndParent, int x, int y, int cx, int cy,
                 std::wstring_view text, bool bold = false) {
    DWORD style = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
    if (bold) style |= SS_CENTER;
    HWND h = CreateWindowExW(0, L"STATIC", nullptr, style,
                             x, y, cx, cy, hwndParent,
                             nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(
                     GetStockObject(DEFAULT_GUI_FONT)), MAKELPARAM(FALSE, 0));
    SetWindowTextW(h, text.data());
    return h;
}

// Create a multi-line readonly edit control for IDs / INF paths.
HWND CreateInfoEdit(HWND hwndParent, int x, int y, int cx, int cy) {
    HWND h = CreateWindowExW(0, L"EDIT", nullptr,
                             WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                             ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL |
                             ES_AUTOVSCROLL | WS_TABSTOP,
                             x, y, cx, cy, hwndParent,
                             nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(
                     GetStockObject(DEFAULT_GUI_FONT)), MAKELPARAM(FALSE, 0));
    SendMessageW(h, EM_SETREADONLY, TRUE, 0);
    SendMessageW(h, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));
    return h;
}

// Copy-to-clipboard helper for a window's text.
void CopyTextToClipboard(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len == 0) return;
    auto buf = std::make_unique<wchar_t[]>(len + 1);
    GetWindowTextW(hwnd, buf.get(), len + 1);
    buf[len] = L'\0';
    std::wstring text(buf.get());

    // Open clipboard
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        // Convert to global memory block
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

DetailsPane::State* DetailsPane::GetState(HWND hwnd) {
    return reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void DetailsPane::SetState(HWND hwnd, State* state) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
}

void DetailsPane::Init(HWND hwnd, HWND hwndMain) {
    State* state = new State();
    state->hwndMain = hwndMain;
    SetState(hwnd, state);

    // Layout will be done by the parent (MainWindow). We create controls here
    // so they persist; the parent positions them.

    // Header: "Device details" label
    CreateLabel(hwnd, 8, 8, 200, 20, L"Device details", true);

    // Description
    CreateLabel(hwnd, 8, 32, 80, 16, Strings::DetDescription);
    auto hDescEdit = CreateInfoEdit(hwnd, 8, 48, 300, 40);

    // Manufacturer
    CreateLabel(hwnd, 8, 92, 100, 16, Strings::DetManufacturer);
    auto hMfgEdit = CreateInfoEdit(hwnd, 8, 108, 300, 24);

    // Instance ID
    CreateLabel(hwnd, 8, 136, 100, 16, Strings::DetInstanceId);
    auto hInstIdEdit = CreateInfoEdit(hwnd, 8, 152, 300, 24);

    // Hardware IDs label
    CreateLabel(hwnd, 8, 180, 100, 16, Strings::DetHardwareIds);
    auto hHwIdEdit = CreateInfoEdit(hwnd, 8, 196, 300, 60);

    // Compatible IDs label
    CreateLabel(hwnd, 8, 256, 100, 16, Strings::DetCompatibleIds);
    auto hCompatIdEdit = CreateInfoEdit(hwnd, 8, 272, 300, 60);

    // Installed INF
    CreateLabel(hwnd, 8, 336, 100, 16, Strings::DetInstalledInf);
    auto hInstInfEdit = CreateInfoEdit(hwnd, 8, 352, 300, 24);

    // Installed version
    CreateLabel(hwnd, 8, 376, 100, 16, Strings::DetInstalledVer);
    auto hInstVerEdit = CreateInfoEdit(hwnd, 8, 392, 300, 24);

    // Separator
    CreateLabel(hwnd, 8, 416, 300, 2, L"", false);

    // Candidate header
    CreateLabel(hwnd, 8, 424, 200, 20, L"Candidate driver", true);

    // Candidate INF
    CreateLabel(hwnd, 8, 448, 100, 16, Strings::DetCandidateInf);
    auto hCandInfEdit = CreateInfoEdit(hwnd, 8, 464, 300, 24);

    // Candidate version
    CreateLabel(hwnd, 8, 488, 100, 16, Strings::DetCandidateVer);
    auto hCandVerEdit = CreateInfoEdit(hwnd, 8, 504, 300, 24);

    // Signer
    CreateLabel(hwnd, 8, 528, 100, 16, Strings::DetSigner);
    auto hSignerEdit = CreateInfoEdit(hwnd, 8, 544, 300, 24);

    // Match score
    CreateLabel(hwnd, 8, 568, 100, 16, Strings::DetMatchScore);
    auto hScoreEdit = CreateInfoEdit(hwnd, 8, 584, 100, 24);

    // Reason tags
    CreateLabel(hwnd, 8, 608, 100, 16, Strings::DetReasonTags);
    auto hReasonsEdit = CreateInfoEdit(hwnd, 8, 624, 300, 60);

    state->hDescEdit = hDescEdit;
    state->hMfgEdit = hMfgEdit;
    state->hInstIdEdit = hInstIdEdit;
    state->hHwIdEdit = hHwIdEdit;
    state->hCompatIdEdit = hCompatIdEdit;
    state->hInstInfEdit = hInstInfEdit;
    state->hInstVerEdit = hInstVerEdit;
    state->hCandInfEdit = hCandInfEdit;
    state->hCandVerEdit = hCandVerEdit;
    state->hSignerEdit = hSignerEdit;
    state->hScoreEdit = hScoreEdit;
    state->hReasonsEdit = hReasonsEdit;

    // Buttons
    state->hInstallBtn = CreateWindowExW(0, L"BUTTON", Strings::DetInstallBtn.data(),
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                          8, 688, 140, 32,
                                          hwnd, reinterpret_cast<HMENU>(IDC_DET_INSTALL),
                                          GetModuleHandleW(nullptr), nullptr);
    state->hChooseAnotherBtn = CreateWindowExW(0, L"BUTTON", Strings::DetChooseAnother.data(),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                                152, 688, 140, 32,
                                                hwnd, reinterpret_cast<HMENU>(IDC_DET_CHOOSE_ANOTHER),
                                                GetModuleHandleW(nullptr), nullptr);

    // Force install checkbox + warning
    state->hForceCheck = CreateWindowExW(0, L"BUTTON", Strings::DetForceInstall.data(),
                                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                                          8, 728, 120, 20,
                                          hwnd, reinterpret_cast<HMENU>(IDC_DET_FORCE),
                                          GetModuleHandleW(nullptr), nullptr);
    state->hForceWarning = CreateLabel(hwnd, 8, 748, 280, 40, Strings::DetForceWarning);
}

void DetailsPane::SetDevice(HWND hwnd, const Hw::Device& device) {
    State* state = GetState(hwnd);
    if (!state) return;
    state->currentDevice = device;

    // Update the controls that depend on device only
    State* ctrls = state;

    SetWindowTextW(ctrls->hDescEdit, device.description.c_str());
    SetWindowTextW(ctrls->hMfgEdit, device.manufacturer.c_str());
    SetWindowTextW(ctrls->hInstIdEdit, device.instanceId.c_str());

    // Hardware IDs as multi-line text
    std::wstring hwIds;
    for (auto& id : device.hardwareIds) {
        hwIds += id + L"\n";
    }
    SetWindowTextW(ctrls->hHwIdEdit, hwIds.c_str());

    // Compatible IDs
    std::wstring compatIds;
    for (auto& id : device.compatibleIds) {
        compatIds += id + L"\n";
    }
    SetWindowTextW(ctrls->hCompatIdEdit, compatIds.c_str());

    // Installed INF
    SetWindowTextW(ctrls->hInstInfEdit,
                   device.installedInf.empty() ? L"(none)" : device.installedInf.c_str());

    // Installed version
    SetWindowTextW(ctrls->hInstVerEdit,
                   device.installedVersion.empty() ? L"(unknown)" : device.installedVersion.c_str());
}

void DetailsPane::SetCandidate(HWND hwnd,
                               const Hw::Device& /*device*/,
                               const std::wstring& candidateInf,
                               const std::wstring& candidateVersion,
                               const std::wstring& signer,
                               int score,
                               std::vector<std::wstring> reasonTags) {
    State* state = GetState(hwnd);
    if (!state) return;
    state->candidateInf = candidateInf;
    state->candidateVersion = candidateVersion;
    state->signer = signer;
    state->score = score;
    state->reasonTags = std::move(reasonTags);

    State* ctrls = state;

    SetWindowTextW(ctrls->hCandInfEdit, candidateInf.empty() ? L"(none)" : candidateInf.c_str());
    SetWindowTextW(ctrls->hCandVerEdit, candidateVersion.empty() ? L"(unknown)" : candidateVersion.c_str());
    SetWindowTextW(ctrls->hSignerEdit, signer.empty() ? L"(not signed)" : signer.c_str());
    SetWindowTextW(ctrls->hScoreEdit, std::to_wstring(score).c_str());

    std::wstring reasons;
    for (auto& tag : state->reasonTags) {
        reasons += tag + L"\n";
    }
    SetWindowTextW(ctrls->hReasonsEdit, reasons.c_str());
}

void DetailsPane::RefreshFromSelection(HWND hwndList, HWND hwndDetails, HWND hwndMain) {
    int sel = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED);
    if (sel == -1) {
        // Clear details pane
        SetDevice(hwndDetails, Hw::Device{});
        SetCandidate(hwndDetails, Hw::Device{}, L"", L"", L"", 0, {});
        return;
    }
    Hw::Device dev = DeviceList::GetDeviceAt(hwndList, sel);
    SetDevice(hwndDetails, dev);
    // Candidate info would come from ranking; for now empty.
    SetCandidate(hwndDetails, dev, L"", L"", L"", 0, {});
}

} // namespace DriverNest::Ui
