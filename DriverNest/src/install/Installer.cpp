#include "Installer.h"
#include "RestorePoint.h"
#include "Verify.h"
#include "Log.h"
#include "Strings.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <memory>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace DriverNest::Install {

using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

namespace {

// Run a command and capture stdout/stderr. Used for pnputil.
struct CommandResult {
    int exitCode = -1;
    std::wstring out;
};

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// Convert a UTF-8/ANSI byte buffer produced by a console tool to wide text.
std::wstring WidenBytes(const std::vector<char>& bytes) {
    if (bytes.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(),
                                      static_cast<int>(bytes.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()),
                        out.data(), n);
    return out;
}

[[nodiscard]] CommandResult RunCommand(std::wstring_view commandLine) {
    CommandResult res;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        return res;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = nullptr;

    PROCESS_INFORMATION pi{};
    std::wstring cmdBuf(commandLine);

    const BOOL created = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr,
                                        TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                        &si, &pi);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        return res;
    }

    std::vector<char> buffer;
    char chunk[4096];
    DWORD read = 0;
    while (ReadFile(readPipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
        buffer.insert(buffer.end(), chunk, chunk + read);
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    res.exitCode = static_cast<int>(exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    res.out = WidenBytes(buffer);
    return res;
}

// Extract published name from pnputil output.
std::wstring ExtractPublishedName(std::wstring_view output) {
    const std::wstring needle = L"Published Name:";
    auto pos = output.find(needle);
    if (pos == std::wstring::npos) return {};
    auto start = pos + needle.size();
    // Skip the padding spaces before the file name.
    while (start < output.size() && (output[start] == L' ' || output[start] == L'\t')) {
        ++start;
    }
    auto end = output.find_first_of(L"\r\n", start);
    if (end == std::wstring::npos) end = output.size();
    return std::wstring(output.substr(start, end - start));
}

// Check if pnputil output indicates success.
bool IsSuccess(std::wstring_view output) {
    return output.find(L"successfully") != std::wstring::npos;
}

} // namespace

std::wstring Installer::InstallInf(const std::filesystem::path& infPath, bool force) {
    // pnputil /add-driver "<inf>" /install [/force]
    std::wstring cmd = L"pnputil.exe /add-driver \"";
    cmd += infPath.wstring();
    cmd += L"\" /install";
    if (force) {
        cmd += L" /force";
    }

    LOG_INFO(L"Installing driver: " + infPath.wstring() + L" via: " + cmd);

    auto result = RunCommand(cmd);

    if (result.exitCode != 0) {
        throw std::runtime_error("pnputil failed with exit code " +
                                 std::to_string(result.exitCode));
    }

    if (!IsSuccess(result.out)) {
        if (result.out.find(L"Access denied") != std::wstring::npos ||
            result.out.find(L"Access is denied") != std::wstring::npos) {
            throw std::runtime_error("Access denied. Run as administrator.");
        }
        if (result.out.find(L"not signed") != std::wstring::npos ||
            result.out.find(L"unsigned") != std::wstring::npos) {
            throw std::runtime_error("Driver is not signed.");
        }
        if (result.out.find(L"already exists") != std::wstring::npos) {
            throw std::runtime_error("This driver package is already in the driver store.");
        }
        throw std::runtime_error("pnputil did not report success.");
    }

    std::wstring publishedName = ExtractPublishedName(result.out);
    if (publishedName.empty()) {
        LOG_WARN(L"pnputil succeeded but did not report a published name.");
    } else {
        LOG_INFO(L"Driver installed. Published name: " + publishedName);
    }
    return publishedName;
}

std::vector<Installer::InstallResult> Installer::InstallBatch(
    const std::vector<std::filesystem::path>& infPaths,
    const std::function<void(std::wstring_view, int, int)>& progressCallback) {
    std::vector<InstallResult> results;
    if (infPaths.empty()) return results;

    const int total = static_cast<int>(infPaths.size());
    if (progressCallback) {
        progressCallback(Strings::InstProgressRestorePoint, 0, total);
    }

    // Create a restore point so a failed batch can be undone.
    RESTOREPOINT_HANDLE rpHandle{};
    bool restorePointCreated = Create(L"DriverNest driver installation", rpHandle);
    if (restorePointCreated) {
        LOG_INFO(L"Restore point created for the install batch.");
    } else {
        LOG_WARN(L"Failed to create a restore point; continuing without one.");
    }

    for (int i = 0; i < total; ++i) {
        const auto& inf = infPaths[static_cast<std::size_t>(i)];

        if (progressCallback) {
            progressCallback(std::wstring(Strings::InstProgressInstallingDriver) +
                                 inf.filename().wstring(),
                             i + 1, total);
        }

        InstallResult res;
        res.infPath = inf;
        try {
            res.publishedName = InstallInf(inf, false);
            res.success = true;

            if (progressCallback) {
                progressCallback(std::wstring(Strings::InstProgressVerifyingDriver) +
                                     inf.filename().wstring(),
                                 i + 1, total);
            }

            if (!VerifyInstallation(inf)) {
                res.success = false;
                res.error = L"Verification failed after installation.";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.error = Widen(e.what());
            LOG_ERROR(L"Install failed for " + inf.wstring() + L": " + res.error);
        }

        results.push_back(std::move(res));
    }

    if (restorePointCreated) {
        Close(rpHandle);
    }

    return results;
}

bool Installer::RollbackInf(const std::filesystem::path& previousInfPath) {
    if (!std::filesystem::exists(previousInfPath)) {
        LOG_WARN(L"Previous INF not found for rollback: " + previousInfPath.wstring());
        return false;
    }

    try {
        InstallInf(previousInfPath, false);
        LOG_INFO(L"Rollback installed: " + previousInfPath.wstring());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(L"Rollback failed: " + Widen(e.what()));
        return false;
    }
}

std::vector<std::wstring> Installer::CleanOldPackages() {
    // Removal is never automatic: the user must choose packages explicitly.
    return {};
}

std::vector<std::pair<std::wstring, std::wstring>> Installer::ListDriverStore() {
    std::vector<std::pair<std::wstring, std::wstring>> packages;

    auto result = RunCommand(L"pnputil.exe /enum-drivers");
    std::wstring output = result.out;

    std::size_t pos = 0;
    while (pos < output.size()) {
        const std::wstring publishedMarker = L"Published Name:";
        auto pubPos = output.find(publishedMarker, pos);
        if (pubPos == std::wstring::npos) break;

        auto pubStart = pubPos + publishedMarker.size();
        while (pubStart < output.size() &&
               (output[pubStart] == L' ' || output[pubStart] == L'\t')) {
            ++pubStart;
        }
        auto pubEnd = output.find_first_of(L"\r\n", pubStart);
        if (pubEnd == std::wstring::npos) break;
        std::wstring publishedName = output.substr(pubStart, pubEnd - pubStart);

        const std::wstring originalMarker = L"Original Name:";
        auto origPos = output.find(originalMarker, pubEnd);
        if (origPos == std::wstring::npos) break;

        auto origStart = origPos + originalMarker.size();
        while (origStart < output.size() &&
               (output[origStart] == L' ' || output[origStart] == L'\t')) {
            ++origStart;
        }
        auto origEnd = output.find_first_of(L"\r\n", origStart);
        if (origEnd == std::wstring::npos) break;

        packages.emplace_back(publishedName,
                              output.substr(origStart, origEnd - origStart));
        pos = origEnd;
    }

    return packages;
}

} // namespace DriverNest::Install
