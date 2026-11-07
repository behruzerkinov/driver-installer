#pragma once
#include "Settings.h"
#include <string>
#include <vector>
#include <windows.h>

namespace DriverNest::Ui {

// ---------------------------------------------------------------------------
// Dialog helpers. All are modal and use DialogBoxParam with a shared
// DLGPROC table. Resource templates live in a dialog resource script
// (src/ui/dialogs.rc) which is included by the main app.rc via #include.
// ---------------------------------------------------------------------------

// --- Confirm install dialog ---
// Shows each selected driver with signer, version change, restore point notice.
void ShowConfirmInstall(HWND hwndOwner,
                        const std::vector<std::pair<std::wstring, std::wstring>>& drivers);

// --- Settings dialog ---
void ShowSettingsDialog(HWND hwndOwner);

// --- About dialog ---
void ShowAboutDialog(HWND hwndOwner);

// --- Undo last batch dialog ---
void ShowUndoDialog(HWND hwndOwner);

// --- Installation progress dialog ---
// Non-modal progress dialog shown during install batches.
struct InstallProgress {
    std::wstring title;
    std::wstring currentTask;   // e.g. "Creating restore point..."
    std::wstring currentFile;   // e.g. "Installing driver: abc.inf"
    int currentStep = 0;
    int totalSteps = 0;
    bool cancelled = false;
};
void ShowInstallProgress(HWND hwndOwner, InstallProgress& progress);
void UpdateInstallProgress(InstallProgress& progress);
void CloseInstallProgress();

// --- Common dialog support ---
std::wstring FormatErrorMessage(DWORD errorCode);
std::wstring FormatVersion(std::wstring_view version);

} // namespace DriverNest::Ui
