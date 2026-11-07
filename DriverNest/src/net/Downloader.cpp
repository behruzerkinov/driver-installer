#include "Downloader.h"
#include "Hash.h"
#include "Log.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace DriverNest::Net {

namespace {

// ---------------------------------------------------------------------------
// Simple JSON string parser (no external dependency per spec).
// Handles "key": "value" and "key": number, "key": [...].
// ---------------------------------------------------------------------------
std::wstring JsonUnescape(std::wstring_view s) {
    std::wstring out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t next = s[i + 1];
            switch (next) {
                case L'"': out += L'"'; ++i; break;
                case L'\\': out += L'\\'; ++i; break;
                case L'/': out += L'/'; ++i; break;
                case L'n': out += L'\n'; ++i; break;
                case L't': out += L'\t'; ++i; break;
                case L'r': out += L'\r'; ++i; break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Find a JSON string value for a given key.
std::wstring FindJsonString(std::string_view json, std::string_view key) {
    std::string searchKey = std::string("\"") + std::string(key) + std::string("\":");
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return {};

    pos += searchKey.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                   json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"') return {};

    auto start = pos + 1;
    auto end = start;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\') ++end; // skip escaped char
        ++end;
    }
    if (end >= json.size()) return {};

    std::string raw(json.substr(start, end - start));
    // Convert to wide and unescape
    int size = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()),
                                   nullptr, 0);
    if (size <= 0) return {};
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()),
                        wstr.data(), size);
    return JsonUnescape(wstr);
}

int64_t FindJsonInt(std::string_view json, std::string_view key) {
    std::string searchKey = std::string("\"") + std::string(key) + std::string("\":");
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return 0;

    pos += searchKey.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                   json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }

    if (pos >= json.size() || (json[pos] != '-' && !isdigit(json[pos]))) return 0;

    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
        num += json[pos];
        ++pos;
    }

    try {
        return std::stoll(num);
    } catch (...) {
        return 0;
    }
}

// Parse hardware IDs array from JSON.
std::vector<std::wstring> FindJsonStringArray(std::string_view json, std::string_view key) {
    std::vector<std::wstring> result;
    std::string searchKey = std::string("\"") + std::string(key) + std::string("\":");
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return result;

    pos += searchKey.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                   json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '[') return result;

    // Find closing bracket
    auto end = json.find(']', pos);
    if (end == std::string::npos) return result;

    std::string arrayContent(json.substr(pos + 1, end - pos - 1));

    // Find all quoted strings
    size_t i = 0;
    while (i < arrayContent.size()) {
        // Skip whitespace and commas
        while (i < arrayContent.size() && (arrayContent[i] == ' ' || arrayContent[i] == ',' ||
                                              arrayContent[i] == '\t' || arrayContent[i] == '\n')) {
            ++i;
        }
        if (i >= arrayContent.size() || arrayContent[i] != '"') break;

        auto strStart = i + 1;
        auto strEnd = strStart;
        while (strEnd < arrayContent.size() && arrayContent[strEnd] != '"') {
            if (arrayContent[strEnd] == '\\') ++strEnd;
            ++strEnd;
        }
        if (strEnd >= arrayContent.size()) break;

        std::string raw(arrayContent.substr(strStart, strEnd - strStart));
        int size = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()),
                                       nullptr, 0);
        if (size > 0) {
            std::wstring wstr(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()),
                                wstr.data(), size);
            result.push_back(JsonUnescape(wstr));
        }
        i = strEnd + 1;
    }

    return result;
}

} // namespace

DownloadResult DownloadFile(const DownloadOptions& options) {
    DownloadResult result;
    result.success = false;

    if (options.url.empty()) {
        result.error = L"URL is empty";
        return result;
    }

    // Open WinHTTP session
    HINTERNET hSession = WinHttpOpen(L"DriverNest/1.0",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      nullptr, nullptr, 0);
    if (!hSession) {
        result.error = L"WinHttpOpen failed";
        return result;
    }

    // Set secure protocols to TLS 1.2 minimum
    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
                           WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
                     &secureProtocols, sizeof(secureProtocols));

    // Parse URL
    std::wstring url = options.url;
    std::wstring host;
    std::wstring path = L"/";
    WORD port = 443;

    auto schemeEnd = url.find(L"://");
    if (schemeEnd != std::wstring::npos) {
        auto hostStart = schemeEnd + 3;
        auto pathStart = url.find(L'/', hostStart);
        if (pathStart != std::wstring::npos) {
            host = url.substr(hostStart, pathStart - hostStart);
            path = url.substr(pathStart);
        } else {
            host = url.substr(hostStart);
        }
    } else {
        host = url;
    }

    // Open connection
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                         port, 0);
    if (!hConnect) {
        result.error = L"WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Open request
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             nullptr, nullptr, nullptr,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        result.error = L"WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0)) {
        result.error = L"WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        result.error = L"WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Check HTTP status
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                        &size, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        result.error = L"HTTP error " + std::to_wstring(statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Get content length
    int64_t totalBytes = 0;
    {
        DWORD dwSize = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH |
                            WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &dwSize,
                            &dwSize, WINHTTP_NO_HEADER_INDEX);
        totalBytes = static_cast<int64_t>(dwSize);
    }

    // Check for Accept-Ranges for resume support
    bool supportsRanges = false;
    {
        wchar_t buffer[256];
        DWORD dwSize = ARRAYSIZE(buffer);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_ACCEPT_RANGES,
                                WINHTTP_HEADER_NAME_BY_INDEX, buffer,
                                &dwSize, WINHTTP_NO_HEADER_INDEX)) {
            supportsRanges = (wcscmp(buffer, L"bytes") == 0);
        }
    }

    // Download to .part file first
    std::filesystem::path partPath = options.destination;
    partPath += L".part";

    // Open file for writing
    HANDLE hFile = CreateFileW(partPath.wstring().c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        result.error = L"Could not create destination file";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Read and write data
    int64_t downloadedBytes = 0;
    auto startTime = std::chrono::steady_clock::now();

    BYTE buffer[64 * 1024]; // 64 KB buffer

    while (true) {
        if (options.cancelFlag && options.cancelFlag->load()) {
            result.error = L"Download cancelled";
            CloseHandle(hFile);
            DeleteFileW(partPath.wstring().c_str());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Read from WinHTTP
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer, ARRAYSIZE(buffer), &bytesRead)) {
            result.error = L"WinHttpReadData failed";
            CloseHandle(hFile);
            DeleteFileW(partPath.wstring().c_str());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        if (bytesRead == 0) break; // EOF

        // Write to file
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, nullptr) ||
            bytesWritten != bytesRead) {
            result.error = L"WriteFile failed";
            CloseHandle(hFile);
            DeleteFileW(partPath.wstring().c_str());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        downloadedBytes += bytesRead;

        // Update progress
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime).count();
        int64_t speed = (elapsed > 0) ? (downloadedBytes * 1000 / elapsed) : 0;

        if (options.progress) {
            options.progress(downloadedBytes, totalBytes, speed);
        }
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Verify SHA-256 if expected hash provided
    if (!options.expectedSha256.empty()) {
        std::string computedHash = Sha256File(partPath.wstring());
        if (computedHash.empty()) {
            result.error = L"Could not compute SHA-256 hash";
            DeleteFileW(partPath.wstring().c_str());
            return result;
        }

        const std::wstring computedWide(computedHash.begin(), computedHash.end());
        if (computedWide != options.expectedSha256) {
            result.error = L"SHA-256 mismatch";
            DeleteFileW(partPath.wstring().c_str());
            return result;
        }
    }

    // Rename .part to final destination
    std::error_code ec;
    std::filesystem::rename(partPath, options.destination, ec);
    if (ec) {
        result.error = L"Could not rename downloaded file: " +
                       std::wstring(ec.message().begin(), ec.message().end());
        DeleteFileW(partPath.wstring().c_str());
        return result;
    }

    result.success = true;
    result.finalPath = options.destination;
    return result;
}

std::vector<ManifestEntry> ParseManifest(std::string_view json) {
    std::vector<ManifestEntry> entries;

    // Find all entries with hardwareIds
    std::string searchKey = "\"hardwareIds\":";
    size_t pos = 0;
    while (pos < json.size()) {
        auto keyPos = json.find(searchKey, pos);
        if (keyPos == std::string::npos) break;

        // Find the enclosing object
        auto objStart = json.rfind('{', keyPos);
        if (objStart == std::string::npos) break;

        auto objEnd = json.find('}', keyPos);
        if (objEnd == std::string::npos) break;

        std::string_view objJson = json.substr(objStart, objEnd - objStart + 1);

        ManifestEntry entry;
        entry.hardwareIds = FindJsonStringArray(objJson, "hardwareIds");
        entry.url = FindJsonString(objJson, "url");
        entry.sha256 = FindJsonString(objJson, "sha256");
        entry.size = FindJsonInt(objJson, "size");
        entry.signer = FindJsonString(objJson, "signer");

        if (!entry.hardwareIds.empty() && !entry.url.empty()) {
            entries.push_back(entry);
        }

        pos = objEnd + 1;
    }

    return entries;
}

} // namespace DriverNest::Net
