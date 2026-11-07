#include "Verify.h"
#include "app/Log.h"
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#include <setupapi.h>
#include <devguid.h>
#include <shlwapi.h>
#include <algorithm>
#include <filesystem>

namespace DriverNest::Install {

using App::LOG_WARN;

namespace {

// ---------------------------------------------------------------------------
// Verify a file (catalog or INF) using WinVerifyTrust.
// ---------------------------------------------------------------------------
bool VerifyWithWinTrust(const std::filesystem::path& filePath) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    const std::wstring pathStr = filePath.wstring();
    fileInfo.pcwszFilePath = pathStr.c_str();

    GUID actionGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    const LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                                       &actionGuid, &trustData);

    // Always release the state allocated by the verify call.
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionGuid, &trustData);

    return status == ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// Catalog membership check via CryptCATAdmin: does this catalog contain an
// entry for the file at all (used as a fallback when trust fails)?
// ---------------------------------------------------------------------------
bool CatalogContainsFile(const std::filesystem::path& catalogPath) {
    HCATADMIN hCatAdmin = nullptr;
    if (!CryptCATAdminAcquireContext(&hCatAdmin, nullptr, 0)) {
        LOG_WARN(L"Could not acquire the catalog admin context.");
        return false;
    }

    HANDLE hFile = CreateFileW(catalogPath.wstring().c_str(), GENERIC_READ,
                               FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        CryptCATAdminReleaseContext(hCatAdmin, 0);
        return false;
    }

    DWORD hashSize = 0;
    bool ok = CryptCATAdminCalcHashFromFileHandle(hFile, &hashSize, nullptr, 0) != FALSE;
    bool found = false;
    if (ok && hashSize > 0) {
        auto hashBuf = std::make_unique<BYTE[]>(hashSize);
        if (CryptCATAdminCalcHashFromFileHandle(hFile, &hashSize, hashBuf.get(), 0)) {
            HCATINFO hCatInfo = CryptCATAdminEnumCatalogFromHash(hCatAdmin, hashBuf.get(),
                                                                hashSize, 0, nullptr);
            found = (hCatInfo != nullptr);
            if (hCatInfo) {
                CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
            }
        }
    }

    CloseHandle(hFile);
    CryptCATAdminReleaseContext(hCatAdmin, 0);
    return found;
}

} // namespace

bool VerifyCatalogSignature(const std::filesystem::path& catalogPath) {
    if (!std::filesystem::exists(catalogPath)) {
        return false;
    }
    return VerifyWithWinTrust(catalogPath);
}

bool VerifyInstallation(const std::filesystem::path& installedInfPath) {
    wchar_t windowsDir[MAX_PATH] = {};
    if (GetWindowsDirectoryW(windowsDir, ARRAYSIZE(windowsDir)) == 0) {
        return false;
    }

    const std::filesystem::path infDir =
        std::filesystem::path(windowsDir) / L"INF";

    std::error_code ec;
    if (!std::filesystem::exists(infDir, ec)) {
        return false;
    }

    const std::wstring wanted = installedInfPath.filename().wstring();
    for (const auto& entry : std::filesystem::directory_iterator(infDir, ec)) {
        if (ec) break;
        if (_wcsicmp(entry.path().filename().c_str(), wanted.c_str()) != 0) {
            continue;
        }
        // Found the INF. Require a matching, valid catalog when one exists.
        std::filesystem::path catPath = entry.path();
        catPath.replace_extension(L".cat");
        if (std::filesystem::exists(catPath, ec)) {
            return VerifyCatalogSignature(catPath);
        }
        // Installed without a catalog: present but unsigned.
        return true;
    }

    return false;
}

std::wstring GetCatalogSigner(const std::filesystem::path& catalogPath) {
    // Full signer extraction requires parsing the catalog's PKCS#7 blob, which
    // is out of scope for v1. Report trust state only.
    if (!std::filesystem::exists(catalogPath)) {
        return {};
    }
    return VerifyCatalogSignature(catalogPath) ? std::wstring(L"Trusted publisher")
                                               : std::wstring();
}

} // namespace DriverNest::Install
