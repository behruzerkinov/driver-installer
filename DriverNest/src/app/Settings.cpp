#include "Settings.h"
#include "Log.h"
#include <shlobj.h>
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <mutex>
#include <stdexcept>
#include <memory>

namespace DriverNest::App {

namespace {

std::mutex settingsMutex;

std::filesystem::path settingsFilePath() {
    return AppDataDirectory() / "settings.ini";
}

std::wstring getIniField(std::wstring_view section, std::wstring_view key,
                         std::wstring_view def) {
    wchar_t buf[1024];
    DWORD len = GetPrivateProfileStringW(section.data(), key.data(), def.data(),
                                         buf, ARRAYSIZE(buf),
                                         settingsFilePath().c_str());
    return std::wstring(buf, len);
}

void setIniField(std::wstring_view section, std::wstring_view key,
                 std::wstring_view value) {
    WritePrivateProfileStringW(section.data(), key.data(), value.data(),
                               settingsFilePath().c_str());
}

} // namespace

// Helper: convert a path to wstring for logging.
std::wstring DirStr(const std::filesystem::path& p) {
    return p.wstring();
}

Settings LoadSettings() {
    Settings s;
    {
        std::lock_guard<std::mutex> guard(settingsMutex);
        std::wstring theme = getIniField(L"General", L"Theme", L"System");
        if (theme == L"Light") s.theme = L"Light";
        else if (theme == L"Dark") s.theme = L"Dark";
        else s.theme = L"System";

        std::wstring logLevel = getIniField(L"General", L"LogLevel", L"Info");
        if (logLevel == L"Error") s.logLevel = L"Error";
        else if (logLevel == L"Warn") s.logLevel = L"Warn";
        else if (logLevel == L"Debug") s.logLevel = L"Debug";
        else s.logLevel = L"Info";

        std::wstring packRoot = getIniField(L"General", L"PackRoot", L"");
        if (!packRoot.empty()) {
            s.packRoot = std::filesystem::path(packRoot);
        }

        std::wstring url = getIniField(L"General", L"OnlineManifestUrl", L"");
        s.onlineManifestUrl = url;

        std::wstring showUnsigned = getIniField(L"General", L"ShowUnsigned", L"0");
        s.showUnsigned = (showUnsigned == L"1");

        std::wstring lang = getIniField(L"General", L"Language", L"en");
        s.language = lang.empty() ? L"en" : lang;

        // Geometry
        s.mainLeft = GetPrivateProfileIntW(L"Window", L"Left", 0,
                                           settingsFilePath().c_str());
        s.mainTop  = GetPrivateProfileIntW(L"Window", L"Top", 0,
                                           settingsFilePath().c_str());
        s.mainWidth  = GetPrivateProfileIntW(L"Window", L"Width", 1100,
                                             settingsFilePath().c_str());
        s.mainHeight = GetPrivateProfileIntW(L"Window", L"Height", 720,
                                             settingsFilePath().c_str());
        std::wstring max = getIniField(L"Window", L"Maximized", L"0");
        s.mainMaximized = (max == L"1");

        std::wstring splitter = getIniField(L"Window", L"SplitterPos", L"0");
        s.splitterPos = std::stoi(splitter);
    }
    return s;
}

void SaveSettings(const Settings& s) {
    std::lock_guard<std::mutex> guard(settingsMutex);

    // Ensure directory exists
    if (!CreateAppDataDirectory()) {
        LOG_WARN(L"Could not create app data directory; settings not saved.");
        return;
    }

    std::wstring packRoot;
    if (!s.packRoot.empty()) {
        packRoot = s.packRoot.wstring();
    }
    setIniField(L"General", L"PackRoot", packRoot);
    setIniField(L"General", L"OnlineManifestUrl", s.onlineManifestUrl);
    setIniField(L"General", L"ShowUnsigned", s.showUnsigned ? L"1" : L"0");
    setIniField(L"General", L"Theme", s.theme);
    setIniField(L"General", L"LogLevel", s.logLevel);
    setIniField(L"General", L"Language", s.language);

    setIniField(L"Window", L"Left", std::to_wstring(s.mainLeft));
    setIniField(L"Window", L"Top", std::to_wstring(s.mainTop));
    setIniField(L"Window", L"Width", std::to_wstring(s.mainWidth));
    setIniField(L"Window", L"Height", std::to_wstring(s.mainHeight));
    setIniField(L"Window", L"Maximized", s.mainMaximized ? L"1" : L"0");
    setIniField(L"Window", L"SplitterPos", std::to_wstring(s.splitterPos));
}

std::filesystem::path AppDataDirectory() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        return std::filesystem::path(path) / "DriverNest";
    }
    return std::filesystem::path(L"C:\\ProgramData\\DriverNest");
}

bool CreateAppDataDirectory() {
    auto dir = AppDataDirectory();
    std::error_code ec;
    if (std::filesystem::exists(dir, ec) && ec) {
        LOG_WARN(L"App data dir exists but could not be queried: " + DirStr(dir));
        return false;
    }
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            LOG_WARN(L"Failed to create app data dir: " + DirStr(dir) + L": " +
                     std::to_wstring(ec.value()));
            return false;
        }
    }

    // Restrict ACL: current user + Administrators only.
    PSID userSid = nullptr;
    PSID adminSid = nullptr;
    EXPLICIT_ACCESSW ea[2];
    PACL dacl = nullptr;

    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        LOG_WARN(L"Could not open process token for ACL setup.");
        return true;
    }

    DWORD ridsize = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &ridsize);
    auto buf = std::make_unique<BYTE[]>(ridsize);
    if (!GetTokenInformation(token, TokenUser, buf.get(), ridsize, &ridsize)) {
        LOG_WARN(L"Could not get token user info.");
        CloseHandle(token);
        return true;
    }
    auto* userInfo = reinterpret_cast<TOKEN_USER*>(buf.get());
    userSid = userInfo->User.Sid;

    // Administrators SID
    ridsize = 0;
    if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, nullptr, &ridsize)) {
        auto adminBuf = std::make_unique<BYTE[]>(ridsize);
        if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr,
                                reinterpret_cast<PSID>(adminBuf.get()), &ridsize)) {
            adminSid = reinterpret_cast<PSID>(adminBuf.get());
        }
    }

    ZeroMemory(&ea, sizeof(ea));
    ea[0].grfAccessPermissions = GENERIC_ALL;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea[0].Trustee.ptstrName = reinterpret_cast<LPWSTR>(userSid);

    if (adminSid) {
        ea[1].grfAccessPermissions = GENERIC_ALL;
        ea[1].grfAccessMode = SET_ACCESS;
        ea[1].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea[1].Trustee.ptstrName = reinterpret_cast<LPWSTR>(adminSid);
    }

    if (SetEntriesInAclW(adminSid ? 2 : 1, ea, nullptr, &dacl) != ERROR_SUCCESS) {
        LOG_WARN(L"SetEntriesInAcl failed.");
        CloseHandle(token);
        return true;
    }

    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE)) {
        LOG_WARN(L"SetSecurityDescriptorDacl failed.");
        LocalFree(dacl);
        CloseHandle(token);
        return true;
    }

    if (SetNamedSecurityInfoW(dir.wstring().data(), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, dacl, nullptr) != ERROR_SUCCESS) {
        LOG_WARN(L"SetNamedSecurityInfo failed for app data dir.");
        LocalFree(dacl);
        LocalFree(sd);
        CloseHandle(token);
        return true;
    }
    LocalFree(dacl);
    LocalFree(sd);
    CloseHandle(token);
    return true;
}

} // namespace DriverNest::App
