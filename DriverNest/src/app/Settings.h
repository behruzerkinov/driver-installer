#pragma once
#include <string>
#include <filesystem>

namespace DriverNest::App {

// ---------------------------------------------------------------------------
// Runtime settings loaded from an INI file (no JSON dependency per spec).
// Stored under %LOCALAPPDATA%\DriverNest\settings.ini with a restricted ACL.
//
// Sections:
//   [General]
//     PackRoot = <absolute folder path or empty>
//     OnlineManifestUrl = <url or empty>
//     ShowUnsigned = 0 | 1
//     Theme = System | Light | Dark
//     LogLevel = Error | Warn | Info | Debug
//     Language = en   (reserved for v1, always "en")
//
// All values are ASCII/UTF-8 where practical; paths stored as absolute UTF-16
// in the INI via WritePrivateProfileStringW read with GetPrivateProfileStringW.
// ---------------------------------------------------------------------------
struct Settings {
    std::filesystem::path packRoot;         // folder tree of driver packs
    std::wstring onlineManifestUrl;         // empty = offline only
    bool showUnsigned = false;              // include unsigned candidates
    std::wstring theme = L"System";         // System | Light | Dark
    std::wstring logLevel = L"Info";        // Error | Warn | Info | Debug
    std::wstring language = L"en";          // reserved (v1)

    // Grid / window geometry persisted from the main window.
    int mainLeft = 0;
    int mainTop = 0;
    int mainWidth = 1100;
    int mainHeight = 720;
    bool mainMaximized = false;
    int splitterPos = 0;                    // 0 = default, set by UI
};

// ---------------------------------------------------------------------------
// Load/save helpers. They must tolerate a missing file (defaults apply) and
// bad values (ignored individually, not fatal).
// ---------------------------------------------------------------------------
[[nodiscard]] Settings LoadSettings();
void SaveSettings(const Settings& s);

// Directory that holds settings, logs, index cache, and journal.
[[nodiscard]] std::filesystem::path AppDataDirectory();

// Create the app data folder with a restricted ACL (current user + Admins).
bool CreateAppDataDirectory();

} // namespace DriverNest::App
