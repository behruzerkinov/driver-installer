#pragma once
#include <string>
#include <windows.h>

namespace DriverNest::Strings {

// ---------------------------------------------------------------------------
// Central place for all English user-facing strings.
// No placeholder text. No TODO comments in shipped code.
// For v1 the only language is English; a language selector is reserved.
// ---------------------------------------------------------------------------

// --- Application identity ---
inline constexpr std::wstring_view AppName        = L"DriverNest";
inline constexpr std::wstring_view AppTagline     = L"Find it. Verify it. Install it.";
inline constexpr std::wstring_view AppVersion     = L"1.4.0";

// --- Main window ---
inline constexpr std::wstring_view WindowTitle    = L"DriverNest";
inline constexpr std::wstring_view SearchPlaceholder = L"Search hardware node, ID, or class...";

// --- Toolbar ---
inline constexpr std::wstring_view BtnScan              = L"Scan Hardware";
inline constexpr std::wstring_view BtnSearchOnline      = L"Search online";
inline constexpr std::wstring_view BtnCancelOnline      = L"Cancel search";
inline constexpr std::wstring_view BtnSelectAll         = L"Select All";
inline constexpr std::wstring_view BtnDeselectAll       = L"Deselect All";
inline constexpr std::wstring_view BtnInstallSelected   = L"Install Selected";
inline constexpr std::wstring_view BtnUndoLastBatch     = L"Recovery Help";
inline constexpr std::wstring_view FilterAll            = L"All";
inline constexpr std::wstring_view FilterMissing        = L"Missing";
inline constexpr std::wstring_view FilterOutdated       = L"Outdated";
inline constexpr std::wstring_view FilterProblems       = L"Problems";
inline constexpr std::wstring_view FilterUpToDate       = L"Up to date";

// --- Device list columns ---
inline constexpr std::wstring_view ColDevice   = L"Device";
inline constexpr std::wstring_view ColStatus   = L"Status";
inline constexpr std::wstring_view ColVersion  = L"Installed version";
inline constexpr std::wstring_view ColCandidate = L"Available version";
inline constexpr std::wstring_view ColSource   = L"Source";

// --- Status bar ---
inline constexpr std::wstring_view StatusPackRootUnknown = L"No pack folder set";
inline constexpr std::wstring_view StatusPackRoot        = L"Pack: ";
inline constexpr std::wstring_view StatusIndexedDrivers  = L"Drivers indexed: ";
inline constexpr std::wstring_view StatusLastScan        = L"Last scan: ";
inline constexpr std::wstring_view StatusScanning        = L"Scanning...";
inline constexpr std::wstring_view StatusIdle            = L"Ready";

// --- Device states ---
inline constexpr std::wstring_view StateOk             = L"OK";
inline constexpr std::wstring_view StateMissing        = L"Missing driver";
inline constexpr std::wstring_view StateOutdated       = L"Outdated";
inline constexpr std::wstring_view StateProblem        = L"Problem";
inline constexpr std::wstring_view StateUnsigned       = L"Unsigned";
inline constexpr std::wstring_view StatePendingRestart  = L"Reboot Required";
inline constexpr std::wstring_view StateQueued          = L"Waiting...";
inline constexpr std::wstring_view StateDownloading     = L"Downloading";
inline constexpr std::wstring_view StateInstalling      = L"Installing...";
inline constexpr std::wstring_view StateVerifying       = L"Verifying...";
inline constexpr std::wstring_view StateInstalled       = L"Installed";
inline constexpr std::wstring_view StateFailed          = L"Failed";

// --- Rescan prompt after installation ---
inline constexpr std::wstring_view DlgRescanPromptTitle = L"Installation Complete";
inline constexpr std::wstring_view DlgRescanPromptBody  = L"Driver installation has finished successfully.\n\nUpdated drivers require a system restart for Windows to load the new kernel drivers.\n\nWould you like to rescan your system now to verify the current hardware state?";
inline constexpr std::wstring_view DlgRescanBtn         = L"Rescan Now";
inline constexpr std::wstring_view DlgCloseBtn          = L"Close";
inline constexpr std::wstring_view StatusRebootPendingNotice = L" driver(s) installed and waiting for restart";
inline constexpr std::wstring_view StatusNeedInstallNotice   = L" driver(s) need installation";
inline constexpr std::wstring_view StatusAllUpToDateNotice   = L"All drivers are up to date (system restart pending).";

// --- Status bar time formatting text ---
inline constexpr std::wstring_view StatusNeverScanned = L"Never";

// --- Details pane ---
inline constexpr std::wstring_view DetDescription   = L"Description";
inline constexpr std::wstring_view DetManufacturer  = L"Manufacturer";
inline constexpr std::wstring_view DetInstanceId    = L"Instance ID";
inline constexpr std::wstring_view DetHardwareIds   = L"Hardware IDs";
inline constexpr std::wstring_view DetCompatibleIds = L"Compatible IDs";
inline constexpr std::wstring_view DetInstalledInf  = L"Installed INF";
inline constexpr std::wstring_view DetInstalledVer  = L"Installed version";
inline constexpr std::wstring_view DetCandidateInf  = L"Candidate INF";
inline constexpr std::wstring_view DetCandidateVer  = L"Candidate version";
inline constexpr std::wstring_view DetMatchScore    = L"Match score";
inline constexpr std::wstring_view DetReasonTags    = L"Why this candidate";
inline constexpr std::wstring_view DetSigner        = L"Signer";
inline constexpr std::wstring_view DetInstallBtn    = L"Install this driver";
inline constexpr std::wstring_view DetChooseAnother = L"Choose another candidate";
inline constexpr std::wstring_view DetForceInstall  = L"Force install";
inline constexpr std::wstring_view DetForceWarning  = L"Force install can break a device if the driver is not compatible. Use only when a normal install fails.";

// --- Reason tags for candidate match ---
inline constexpr std::wstring_view ReasonExactId       = L"Exact hardware ID";
inline constexpr std::wstring_view ReasonCompatibleId = L"Compatible hardware ID";
inline constexpr std::wstring_view ReasonNewerVersion  = L"Newer version";
inline constexpr std::wstring_view ReasonNewerDate     = L"Newer driver date";
inline constexpr std::wstring_view ReasonSigned        = L"Signed";
inline constexpr std::wstring_view ReasonClassMatch    = L"Class matches device";
inline constexpr std::wstring_view ReasonManufacturer = L"Provider matches manufacturer";
inline constexpr std::wstring_view ReasonProblemDevice = L"Problem device";
inline constexpr std::wstring_view ReasonUnsigned      = L"Unsigned / no catalog";

// --- Confirmation dialog ---
inline constexpr std::wstring_view DlgConfirmTitle       = L"Confirm driver installation";
inline constexpr std::wstring_view DlgConfirmBody        = L"You are about to install the selected local driver packages.\n\nReview the package version and signature status for each device. Windows may require a restart to apply the driver.";
inline constexpr std::wstring_view DlgConfirmInstall     = L"Install";
inline constexpr std::wstring_view DlgConfirmCancel      = L"Cancel";

// --- Settings dialog ---
inline constexpr std::wstring_view DlgSettingsTitle    = L"Settings";
inline constexpr std::wstring_view DlgSettingsGeneral = L"General";
inline constexpr std::wstring_view SettingsPackRoot    = L"Driver pack folder";
inline constexpr std::wstring_view SettingsPackRootBrowse = L"Browse...";
inline constexpr std::wstring_view SettingsPackRootNone = L"(none)";
inline constexpr std::wstring_view SettingsOnlineUrl  = L"Online manifest URL";
inline constexpr std::wstring_view SettingsOnlineUrlHint = L"Leave empty to work offline only.";
inline constexpr std::wstring_view SettingsShowUnsigned = L"Show unsigned driver candidates";
inline constexpr std::wstring_view SettingsShowUnsignedHint = L"Unsigned drivers are hidden by default. Enable to see them; each still requires explicit confirmation.";
inline constexpr std::wstring_view SettingsTheme      = L"Theme";
inline constexpr std::wstring_view SettingsThemeSystem = L"Follow system";
inline constexpr std::wstring_view SettingsThemeLight = L"Light";
inline constexpr std::wstring_view SettingsThemeDark  = L"Dark";
inline constexpr std::wstring_view SettingsLogLevel   = L"Log level";
inline constexpr std::wstring_view SettingsLanguage   = L"Language";
inline constexpr std::wstring_view SettingsLanguagePlaceholder = L"English (v1)";
inline constexpr std::wstring_view DlgSettingsOk      = L"OK";
inline constexpr std::wstring_view DlgSettingsCancel  = L"Cancel";

// --- About dialog ---
inline constexpr std::wstring_view DlgAboutTitle      = L"About DriverNest";
inline constexpr std::wstring_view DlgAboutVersion    = L"Version ";
inline constexpr std::wstring_view DlgAboutLicense    = L"License: MIT";
inline constexpr std::wstring_view DlgAboutProjectUrl = L"Project";
inline constexpr std::wstring_view DlgAboutCopyDiag   = L"Copy diagnostics";
inline constexpr std::wstring_view DlgAboutCopyDone   = L"Diagnostics copied to clipboard.";
inline constexpr std::wstring_view DlgAboutClose      = L"Close";

// --- Install progress ---
inline constexpr std::wstring_view InstProgressTitle   = L"Installing drivers";
inline constexpr std::wstring_view InstProgressInstalling  = L"Installing";
inline constexpr std::wstring_view InstProgressVerifying   = L"Verifying";
inline constexpr std::wstring_view InstProgressDone        = L"Done";
inline constexpr std::wstring_view InstProgressFailed      = L"Failed";
inline constexpr std::wstring_view InstProgressCancelled   = L"Cancelled";
inline constexpr std::wstring_view InstProgressRestorePoint = L"Creating restore point...";
inline constexpr std::wstring_view InstProgressInstallingDriver = L"Installing driver: ";
inline constexpr std::wstring_view InstProgressVerifyingDriver = L"Verifying driver: ";
inline constexpr std::wstring_view InstProgressSuccess     = L"Driver installed successfully.";
inline constexpr std::wstring_view InstProgressError       = L"Windows could not install this driver. Check that the package is signed and that Secure Boot is not blocking unsigned drivers.";
inline constexpr std::wstring_view InstProgressOfflineWarning = L"Working offline. Online manifest is disabled.";

// --- Errors ---
inline constexpr std::wstring_view ErrNotElevated   = L"DriverNest requires administrator privileges.";
inline constexpr std::wstring_view ErrAnotherInstance = L"DriverNest is already running.";
inline constexpr std::wstring_view ErrNoPackRoot    = L"No driver pack folder is set. Set one in Settings to find driver candidates.";
inline constexpr std::wstring_view ErrScanFailed    = L"Device scan failed. Check the log for details.";
inline constexpr std::wstring_view ErrIndexFailed   = L"Could not load the driver index. Rebuilding...";
inline constexpr std::wstring_view ErrInstallFailed = L"Installation failed for one or more drivers. See the log for details.";
inline constexpr std::wstring_view ErrUndoFailed    = L"Driver recovery is unavailable. Use Device Manager or Windows System Restore.";

// --- Context menu / list actions ---
inline constexpr std::wstring_view CtxRefresh   = L"Refresh (F5)";
inline constexpr std::wstring_view CtxInstall   = L"Install";
inline constexpr std::wstring_view CtxUninstall = L"Uninstall selected drivers";
inline constexpr std::wstring_view CtxSelectAll = L"Select all (Ctrl+A)";
inline constexpr std::wstring_view CtxClearSel  = L"Clear selection (Del)";

// --- Diagnostics ---
inline constexpr std::wstring_view DiagTitle    = L"DriverNest diagnostics";
inline constexpr std::wstring_view DiagVersion  = L"Version: ";
inline constexpr std::wstring_view DiagOsBuild = L"OS build: ";
inline constexpr std::wstring_view DiagDevices = L"Device count: ";
inline constexpr std::wstring_view DiagIndex   = L"Index entries: ";
inline constexpr std::wstring_view DiagPacks   = L"Pack folders: ";
inline constexpr std::wstring_view DiagOnline  = L"Online manifest: ";
inline constexpr std::wstring_view DiagOffline = L"Offline mode";
inline constexpr std::wstring_view DiagTheme   = L"Theme: ";
inline constexpr std::wstring_view DiagElevated = L"Elevated: yes";

// --- Settings not available / placeholders ---
// (All placeholders are prohibited by spec; this section is empty.)

} // namespace DriverNest::Strings
