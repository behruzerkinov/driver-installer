#include "OnlineIndex.h"
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace DriverNest::Net {
namespace {

constexpr std::wstring_view kIndexRoot = L"https://windriverindex.tucny.com/v1/latest";
constexpr DWORD kMaxResponseBytes = 1024 * 1024;

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::optional<std::string> FetchText(const std::wstring& url, const std::atomic<bool>& cancel) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) return std::nullopt;

    HINTERNET session = WinHttpOpen(L"DriverNest/1.4 (https://github.com/behruzerkinov/driver-installer)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return std::nullopt;
    WinHttpSetTimeouts(session, 2500, 2500, 4500, 4500);

    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return std::nullopt;
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    std::optional<std::string> result;
    if (!cancel.load() && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) && status == 200) {
            std::string body;
            std::array<char, 16384> buffer{};
            bool ok = true;
            while (!cancel.load()) {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available)) { ok = false; break; }
                if (available == 0) break;
                if (body.size() + available > kMaxResponseBytes) { ok = false; break; }
                DWORD read = 0;
                const DWORD take = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
                if (!WinHttpReadData(request, buffer.data(), take, &read)) { ok = false; break; }
                body.append(buffer.data(), read);
            }
            if (ok && !cancel.load() && !body.empty()) result = std::move(body);
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

std::vector<std::string_view> ArrayObjects(std::string_view json, std::string_view arrayName) {
    std::vector<std::string_view> out;
    const std::string key = "\"" + std::string(arrayName) + "\"";
    size_t pos = json.find(key);
    if (pos == std::string_view::npos) return out;
    pos = json.find('[', pos + key.size());
    if (pos == std::string_view::npos) return out;
    for (++pos; pos < json.size();) {
        while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ',')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '{') return {};
        const size_t start = pos;
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (; pos < json.size(); ++pos) {
            const char ch = json[pos];
            if (inString) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') inString = false;
                continue;
            }
            if (ch == '"') inString = true;
            else if (ch == '{') ++depth;
            else if (ch == '}' && --depth == 0) {
                out.push_back(json.substr(start, pos - start + 1));
                ++pos;
                break;
            }
        }
        if (depth != 0) return {};
    }
    return out;
}

std::string JsonString(std::string_view object, std::string_view name) {
    const std::string key = "\"" + std::string(name) + "\"";
    size_t pos = object.find(key);
    if (pos == std::string_view::npos) return {};
    pos = object.find(':', pos + key.size());
    if (pos == std::string_view::npos) return {};
    pos = object.find('"', pos + 1);
    if (pos == std::string_view::npos) return {};
    std::string result;
    for (++pos; pos < object.size(); ++pos) {
        char ch = object[pos];
        if (ch == '"') return result;
        if (ch == '\\' && pos + 1 < object.size()) {
            ch = object[++pos];
            if (ch == 'n') result += '\n';
            else if (ch == 'r') result += '\r';
            else if (ch == 't') result += '\t';
            else result += ch;
        } else result += ch;
    }
    return {};
}

int JsonInt(std::string_view object, std::string_view name) {
    const std::string key = "\"" + std::string(name) + "\"";
    size_t pos = object.find(key);
    if (pos == std::string_view::npos) return 0;
    pos = object.find(':', pos + key.size());
    if (pos == std::string_view::npos) return 0;
    while (++pos < object.size() && std::isspace(static_cast<unsigned char>(object[pos]))) {}
    int value = 0;
    while (pos < object.size() && std::isdigit(static_cast<unsigned char>(object[pos]))) {
        value = value * 10 + (object[pos++] - '0');
    }
    return value;
}

std::wstring HardwareIdPath(std::wstring id) {
    std::wstring path;
    for (wchar_t ch : id) {
        if (ch == L'\\') path += L'_';
        else if (ch == L'&') path += L'+';
        else if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                 (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'+') path += ch;
        else {
            constexpr wchar_t hex[] = L"0123456789ABCDEF";
            path += L'%';
            path += hex[(static_cast<unsigned>(ch) >> 4) & 0xF];
            path += hex[static_cast<unsigned>(ch) & 0xF];
        }
    }
    return path;
}

bool AllowedVendorUrl(std::string_view url) {
    if (!url.starts_with("https://")) return false;
    const size_t hostStart = 8;
    const size_t end = url.find_first_of("/:?#", hostStart);
    std::string host(url.substr(hostStart, end == std::string_view::npos ? url.size() - hostStart : end - hostStart));
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (host.find('@') != std::string::npos || host.find('\\') != std::string::npos) return false;
    constexpr std::array<std::string_view, 13> suffixes = {
        ".asus.com", ".msi.com", ".gigabyte.com", ".asrock.com", ".dell.com",
        ".hp.com", ".lenovo.com", ".intel.com", ".amd.com", ".nvidia.com",
        ".realtek.com", ".microsoft.com", ".windows.com"
    };
    if (host == "catalog.update.microsoft.com") return true;
    return std::any_of(suffixes.begin(), suffixes.end(), [&host](std::string_view suffix) {
        return host == suffix.substr(1) || (host.size() > suffix.size() && host.ends_with(suffix));
    });
}

std::string ManufacturerHint(std::wstring_view manufacturer) {
    std::wstring lower(manufacturer);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    constexpr std::array<std::pair<std::wstring_view, std::string_view>, 11> names = {{
        {L"asus", "asus"}, {L"msi", "msi"}, {L"gigabyte", "gigabyte"},
        {L"asrock", "asrock"}, {L"dell", "dell"}, {L"hewlett-packard", "hp"},
        {L"hp", "hp"}, {L"lenovo", "lenovo"}, {L"intel", "intel"},
        {L"amd", "amd"}, {L"nvidia", "nvidia"}
    }};
    for (const auto& [needle, vendor] : names) if (lower.find(needle) != std::wstring::npos) return std::string(vendor);
    if (lower.find(L"realtek") != std::wstring::npos) return "realtek";
    return {};
}

std::optional<OnlineDriverCandidate> SearchOne(const Hw::Device& device, const std::atomic<bool>& cancel) {
    const size_t idCount = std::min<size_t>(2, device.hardwareIds.size());
    for (size_t i = 0; i < idCount && !cancel.load(); ++i) {
        const std::wstring byHwidUrl = std::wstring(kIndexRoot) + L"/by-hwid/" + HardwareIdPath(device.hardwareIds[i]) + L".json";
        const auto hwidJson = FetchText(byHwidUrl, cancel);
        if (!hwidJson) continue;
        const auto familyObjects = ArrayObjects(*hwidJson, "families");
        if (familyObjects.empty()) continue;
        const std::string manufacturer = ManufacturerHint(device.manufacturer);
        std::optional<OnlineDriverCandidate> bestForId;
        int bestIdScore = -100;
        std::string bestIdDate;
        // The index orders families by match_share. Inspect the first two claims
        // so a generic HWID does not prematurely select a less-relevant family.
        const size_t familyCount = std::min<size_t>(2, familyObjects.size());
        for (size_t familyIndex = 0; familyIndex < familyCount && !cancel.load(); ++familyIndex) {
        const auto familyObject = familyObjects[familyIndex];
        const int familyId = JsonInt(familyObject, "family_id");
        if (familyId <= 0) continue;
        const std::wstring byFamilyUrl = std::wstring(kIndexRoot) + L"/by-family/" + std::to_wstring(familyId) + L".json";
        const auto familyJson = FetchText(byFamilyUrl, cancel);
        if (!familyJson) continue;
        const std::string family = JsonString(familyObject, "family");
        std::optional<OnlineDriverCandidate> best;
        int bestScore = -100;
        std::string bestDate;
        for (auto row : ArrayObjects(*familyJson, "known_versions")) {
            const auto url = JsonString(row, "url");
            if (!AllowedVendorUrl(url)) continue;
            const auto vendor = JsonString(row, "vendor");
            const auto version = JsonString(row, "version_raw");
            const auto releaseDate = JsonString(row, "release_date");
            if (url.empty() || version.empty()) continue;
            int score = 0;
            if (!manufacturer.empty() && vendor == manufacturer) score += 5;
            const auto query = url.find_first_of("?#");
            const std::string_view cleanUrl(url.data(), query == std::string::npos ? url.size() : query);
            const auto pathStart = cleanUrl.find('/', 8);
            const auto path = pathStart == std::string_view::npos ? std::string_view{} : cleanUrl.substr(pathStart);
            const auto dot = path.find_last_of('.');
            const auto slash = path.find_last_of('/');
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
                std::string ext(path.substr(dot));
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ext == ".zip" || ext == ".exe" || ext == ".cab" || ext == ".msi") score += 4;
                if (ext == ".xml") continue;
            }
            if (vendor == "wucatalog") score += 1;
            if (releaseDate < bestDate || (releaseDate == bestDate && score < bestScore)) continue;
            bestScore = score;
            bestDate = releaseDate;
            best = OnlineDriverCandidate{
                device.instanceId,
                Utf8ToWide(family),
                Utf8ToWide(version),
                Utf8ToWide(vendor),
                Utf8ToWide(releaseDate),
                Utf8ToWide(url)
            };
        }
        if (best && (bestDate > bestIdDate || (bestDate == bestIdDate && bestScore > bestIdScore))) {
            bestIdScore = bestScore;
            bestIdDate = std::move(bestDate);
            bestForId = std::move(best);
        }
        }
        if (bestForId) return bestForId;
    }
    return std::nullopt;
}

} // namespace

std::vector<OnlineDriverCandidate> SearchOnlineIndex(const std::vector<Hw::Device>& devices,
                                                       const std::atomic<bool>& cancel) {
    std::vector<std::optional<OnlineDriverCandidate>> found(devices.size());
    std::atomic_size_t next{0};
    const size_t threadCount = std::min<size_t>(4, devices.size());
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (size_t worker = 0; worker < threadCount; ++worker) {
        workers.emplace_back([&] {
            for (;;) {
                if (cancel.load()) return;
                const size_t i = next.fetch_add(1);
                if (i >= devices.size()) return;
                found[i] = SearchOne(devices[i], cancel);
            }
        });
    }
    for (auto& worker : workers) worker.join();

    std::vector<OnlineDriverCandidate> results;
    for (auto& item : found) if (item) results.push_back(std::move(*item));
    return results;
}

} // namespace DriverNest::Net
