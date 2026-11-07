#pragma once
#include "DeviceInfo.h"
#include <string>
#include <memory>
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Details pane (right side of the body split).
//
// Shows:
//  - Full description, manufacturer, instance ID (copyable)
//  - Hardware and compatible IDs lists
//  - Installed INF + version
//  - Candidate INF + signer + match score + reason tags
//  - Buttons: Install this, Choose another candidate, Force install (checkbox)
// ---------------------------------------------------------------------------
class DetailsPane {
public:
    static void Init(HWND hwnd, HWND hwndMain);

    // Display a device in the details pane.
    static void SetDevice(HWND hwnd, const Hw::Device& device);

    // Show candidate info (INF, version, signer, score, reasons) for the given
    // device. Called after ranking.
    static void SetCandidate(HWND hwnd,
                             const Hw::Device& device,
                             const std::wstring& candidateInf,
                             const std::wstring& candidateVersion,
                             const std::wstring& signer,
                             int score,
                             std::vector<std::wstring> reasonTags);

    // Refresh the details pane from the selected list item.
    static void RefreshFromSelection(HWND hwndList, HWND hwndDetails, HWND hwndMain);

private:
    struct State {
        HWND hwndMain = nullptr;
        Hw::Device currentDevice;
        std::wstring candidateInf;
        std::wstring candidateVersion;
        std::wstring signer;
        int score = 0;
        std::vector<std::wstring> reasonTags;

        // Child controls created by Init().
        HWND hDescEdit = nullptr;
        HWND hMfgEdit = nullptr;
        HWND hInstIdEdit = nullptr;
        HWND hHwIdEdit = nullptr;
        HWND hCompatIdEdit = nullptr;
        HWND hInstInfEdit = nullptr;
        HWND hInstVerEdit = nullptr;
        HWND hCandInfEdit = nullptr;
        HWND hCandVerEdit = nullptr;
        HWND hSignerEdit = nullptr;
        HWND hScoreEdit = nullptr;
        HWND hReasonsEdit = nullptr;
        HWND hInstallBtn = nullptr;
        HWND hChooseAnotherBtn = nullptr;
        HWND hForceCheck = nullptr;
        HWND hForceWarning = nullptr;
    };

    static State* GetState(HWND hwnd);
    static void SetState(HWND hwnd, State* state);

    // Helpers
    static void UpdateControls(HWND hwnd, const State& state);
};

} // namespace DriverNest::Ui
