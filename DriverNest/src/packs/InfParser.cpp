#include "InfParser.h"
#include "DeviceInfo.h"
#include "Log.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cwctype>
#include <cctype>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

namespace DriverNest::Packs {

using App::LOG_DEBUG;
using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

namespace {

// Maximum file size and line length per spec.
constexpr std::uint32_t MaxInfFileSize = 16 * 1024 * 1024; // 16 MB
constexpr std::size_t MaxLineLength = 8192;

// One section of the INF, in file order.
struct Section {
    std::wstring name;
    std::vector<std::wstring> lines;
};

// Helper: read whole file into a byte buffer.
[[nodiscard]] std::unique_ptr<BYTE[]> ReadFileBytes(std::filesystem::path path,
                                                     std::uint32_t* outSize) {
    if (!std::filesystem::exists(path)) return nullptr;
    auto size = std::filesystem::file_size(path);
    if (size > MaxInfFileSize) {
        LOG_WARN(L"INF file exceeds 16 MB limit: " + path.wstring());
        return nullptr;
    }
    auto buf = std::make_unique<BYTE[]>(size);
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    file.read(reinterpret_cast<char*>(buf.get()), static_cast<std::streamsize>(size));
    if (file.gcount() != static_cast<std::streamsize>(size)) {
        return nullptr;
    }
    if (outSize) *outSize = static_cast<std::uint32_t>(size);
    return buf;
}

// Encoding detection via BOM.
InfEncoding DetectEncoding(std::uint32_t size, const BYTE* data) {
    if (size >= 2) {
        if (data[0] == 0xFF && data[1] == 0xFE) return InfEncoding::Utf16Le;
        if (data[0] == 0xFE && data[1] == 0xFF) return InfEncoding::Utf16Be;
    }
    if (size >= 3) {
        if (data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            return InfEncoding::Utf8;
        }
    }
    return InfEncoding::Ansi;
}

// Convert a wide string view to UTF-8 for storage.
std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                   nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string buf(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        buf.data(), size, nullptr, nullptr);
    return buf;
}

// Convert UTF-8 to wide for processing.
std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                   nullptr, 0);
    if (size <= 0) return {};
    std::wstring buf(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        buf.data(), size);
    return buf;
}

// Trim whitespace from both ends.
std::wstring Trim(std::wstring s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](wchar_t c) { return !std::iswspace(c); }).base(), s.end());
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    [](wchar_t c) { return !std::iswspace(c); }));
    return s;
}

// ASCII-only lowercase, used for section-name and ID comparisons.
std::wstring LowerAscii(std::wstring_view s) {
    std::wstring out(s);
    for (wchar_t& c : out) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return out;
}

// Resolve %StringKey% references in a string using the Strings section map.
std::wstring ResolveStrings(std::wstring_view str,
                            const std::unordered_map<std::wstring, std::wstring>& strings) {
    std::wstring out;
    std::size_t pos = 0;
    while (pos < str.size()) {
        std::size_t start = str.find(L'%', pos);
        if (start == std::wstring_view::npos) {
            out += str.substr(pos);
            break;
        }
        out += str.substr(pos, start - pos);
        std::size_t end = str.find(L'%', start + 1);
        if (end == std::wstring_view::npos) {
            out += str.substr(start);
            break;
        }
        const std::wstring key(str.substr(start + 1, end - start - 1));
        auto it = strings.find(key);
        if (it != strings.end()) {
            out += it->second;
        } else {
            out += str.substr(start, end - start + 1);
        }
        pos = end + 1;
    }
    return out;
}

// Split on commas, trimming each field and stripping surrounding quotes.
std::vector<std::wstring> SplitFields(std::wstring_view s) {
    std::vector<std::wstring> fields;
    std::size_t start = 0;
    while (start <= s.size()) {
        std::size_t comma = s.find(L',', start);
        std::wstring field = Trim(std::wstring(
            s.substr(start, comma == std::wstring_view::npos ? std::wstring_view::npos
                                                             : comma - start)));
        if (field.size() >= 2 && field.front() == L'"' && field.back() == L'"') {
            field = field.substr(1, field.size() - 2);
        }
        fields.push_back(std::move(field));
        if (comma == std::wstring_view::npos) break;
        start = comma + 1;
    }
    return fields;
}

// Parse a version string "MM/DD/YYYY,version" into date and version.
void ParseDriverVer(std::wstring_view ver,
                    std::wstring& dateOut,
                    std::wstring& versionOut) {
    std::wstring cleaned = Trim(std::wstring(ver));
    std::size_t comma = cleaned.find(L',');
    if (comma != std::wstring::npos) {
        dateOut = Trim(cleaned.substr(0, comma));
        versionOut = Trim(cleaned.substr(comma + 1));
    } else {
        versionOut = cleaned;
        dateOut.clear();
    }
}

// Parse a date "MM/DD/YYYY" into FILETIME.
FILETIME ParseDate(std::wstring_view date) {
    FILETIME ft{};
    std::wstring cleaned = Trim(std::wstring(date));
    if (cleaned.size() < 10) return ft;

    const int month = _wtoi(cleaned.substr(0, 2).c_str());
    const int day = _wtoi(cleaned.substr(3, 2).c_str());
    const int year = _wtoi(cleaned.substr(6, 4).c_str());
    if (month <= 0 || day <= 0 || year <= 0) return ft;

    SYSTEMTIME st{};
    st.wYear = static_cast<WORD>(year);
    st.wMonth = static_cast<WORD>(month);
    st.wDay = static_cast<WORD>(day);
    if (!SystemTimeToFileTime(&st, &ft)) {
        ft.dwLowDateTime = 0;
        ft.dwHighDateTime = 0;
    }
    return ft;
}

// Build number of the running OS (0 when it cannot be determined).
DWORD HostBuildNumber() {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi))) {
        return osvi.dwBuildNumber;
    }
    return 0;
}

// Describe how well a decorated model section matches this machine.
struct SectionMatch {
    bool matches = false;
    int archScore = 0;    // 3 = exact arch, 1 = generic "NT"
    DWORD build = 0;      // build number from the decoration, 0 when absent
    bool hasBuild = false;
};

SectionMatch MatchModelSection(std::wstring_view sectionLower,
                               std::wstring_view baseLower,
                               DWORD hostBuild) {
    SectionMatch m;
    if (sectionLower == baseLower) {
        // Undecorated section: valid fallback for every architecture.
        m.matches = true;
        m.archScore = 1;
        return m;
    }
    if (sectionLower.size() <= baseLower.size() ||
        sectionLower.compare(0, baseLower.size(), baseLower) != 0 ||
        sectionLower[baseLower.size()] != L'.') {
        return m;
    }

    std::wstring_view suffix = sectionLower.substr(baseLower.size() + 1);
    std::vector<std::wstring> tokens = SplitFields(std::wstring_view(suffix.data(), suffix.size()));
    // Fields were comma-split; the suffix uses dots, so re-split on '.'.
    tokens.clear();
    std::size_t start = 0;
    while (start <= suffix.size()) {
        std::size_t dot = suffix.find(L'.', start);
        tokens.emplace_back(suffix.substr(start, dot == std::wstring_view::npos
                                                    ? std::wstring_view::npos
                                                    : dot - start));
        if (dot == std::wstring_view::npos) break;
        start = dot + 1;
    }

    m.matches = true;
    std::size_t index = 0;
    if (!tokens.empty() && tokens[0].rfind(L"nt", 0) == 0) {
        if (tokens[0] == L"ntamd64" || tokens[0] == L"ntia64") {
            m.archScore = 3;
        } else if (tokens[0] == L"ntarm64") {
            m.archScore = 0; // wrong architecture for our x64 build
        } else if (tokens[0] == L"ntx86" || tokens[0] == L"ntarm") {
            m.archScore = 0;
        } else {
            m.archScore = 1; // generic "NT"
        }
        index = 1;
    } else {
        m.archScore = 1; // no architecture token at all
    }

    // Remaining numeric tokens are major.minor.build[...]; the build matters.
    DWORD build = 0;
    bool haveBuild = false;
    for (; index < tokens.size(); ++index) {
        if (tokens[index].empty()) continue;
        if (tokens[index].find_first_not_of(L"0123456789") != std::wstring::npos) {
            continue;
        }
        build = static_cast<DWORD>(_wtoi(tokens[index].c_str()));
        haveBuild = true;
    }
    if (haveBuild) {
        if (hostBuild != 0 && build > hostBuild) {
            m.matches = false; // requires a newer OS than this machine
            return m;
        }
        m.build = build;
        m.hasBuild = true;
    }
    return m;
}

// Parse one models-section line: "description = install-section, hwid [, compat...]"
bool ParseModelLine(std::wstring_view line,
                    const std::unordered_map<std::wstring, std::wstring>& strings,
                    const std::wstring& sectionName,
                    InfEntry::Model& model) {
    std::size_t eq = line.find(L'=');
    if (eq == std::wstring_view::npos) return false;

    model.description = ResolveStrings(Trim(std::wstring(line.substr(0, eq))), strings);
    model.sectionName = sectionName;

    const std::vector<std::wstring> fields = SplitFields(line.substr(eq + 1));
    if (fields.empty() || fields[0].empty()) return false;

    bool firstId = true;
    for (std::size_t i = 1; i < fields.size(); ++i) {
        std::wstring id = fields[i];
        if (id.empty()) continue;
        if (id.front() == L'%') id = ResolveStrings(id, strings);
        if (id.empty()) continue;
        if (firstId) {
            model.hardwareIds.push_back(id);
            firstId = false;
        } else {
            model.compatibleIds.push_back(id);
        }
    }
    return !model.hardwareIds.empty();
}

} // namespace

std::vector<InfEntry> ParseInfFile(const std::filesystem::path& path) {
    std::vector<InfEntry> result;

    auto fileSize = std::uint32_t{0};
    auto data = ReadFileBytes(path, &fileSize);
    if (!data) return result;

    const InfEncoding enc = DetectEncoding(fileSize, data.get());

    // Convert to a wide string for parsing.
    std::wstring content;
    switch (enc) {
        case InfEncoding::Utf16Le: {
            if (fileSize < 2) return result;
            const std::uint32_t wcharCount = (fileSize - 2) / 2;
            content.resize(wcharCount);
            memcpy(content.data(), data.get() + 2, wcharCount * sizeof(wchar_t));
            break;
        }
        case InfEncoding::Utf16Be: {
            if (fileSize < 2) return result;
            const std::uint32_t wcharCount = (fileSize - 2) / 2;
            content.resize(wcharCount);
            for (std::uint32_t i = 0; i < wcharCount; ++i) {
                const BYTE hi = data.get()[2 + i * 2];
                const BYTE lo = data.get()[2 + i * 2 + 1];
                content[i] = static_cast<wchar_t>((static_cast<std::uint16_t>(hi) << 8) |
                                                  static_cast<std::uint16_t>(lo));
            }
            break;
        }
        case InfEncoding::Utf8: {
            std::string_view sv(reinterpret_cast<const char*>(data.get() + 3),
                                fileSize - 3);
            content = Utf8ToWide(sv);
            break;
        }
        case InfEncoding::Ansi: {
            std::string_view sv(reinterpret_cast<const char*>(data.get()), fileSize);
            const int size = MultiByteToWideChar(CP_ACP, 0, sv.data(),
                                                 static_cast<int>(sv.size()), nullptr, 0);
            if (size > 0) {
                content.resize(size);
                MultiByteToWideChar(CP_ACP, 0, sv.data(),
                                    static_cast<int>(sv.size()), content.data(), size);
            }
            break;
        }
    }

    // ---- Pass 1: split into sections, honouring line continuations ----------
    std::vector<Section> sections;
    std::unordered_map<std::wstring, std::size_t> sectionIndex; // lowercase name
    std::unordered_map<std::wstring, std::wstring> strings;
    InfEntry entry;
    entry.path = path;

    std::wstring currentSectionName;
    std::size_t currentSection = std::size_t(-1);
    std::wstring logical;

    auto flushLogical = [&]() {
        if (logical.empty() || currentSection == std::size_t(-1)) {
            logical.clear();
            return;
        }
        sections[currentSection].lines.push_back(logical);
        logical.clear();
    };

    std::size_t lineStart = 0;
    while (lineStart < content.size()) {
        std::size_t lineEnd = content.find_first_of(L"\r\n", lineStart);
        if (lineEnd == std::wstring::npos) lineEnd = content.size();

        std::wstring line = Trim(content.substr(lineStart, lineEnd - lineStart));
        lineStart = lineEnd + 1;

        if (!line.empty() && line.front() == L'[') {
            flushLogical();
            const std::size_t close = line.find(L']');
            if (close != std::wstring::npos) {
                currentSectionName = Trim(line.substr(1, close - 1));
                const std::wstring key = LowerAscii(currentSectionName);
                auto it = sectionIndex.find(key);
                if (it == sectionIndex.end()) {
                    sectionIndex.emplace(key, sections.size());
                    sections.push_back(Section{currentSectionName, {}});
                    currentSection = sections.size() - 1;
                } else {
                    currentSection = it->second;
                }
            }
            continue;
        }

        if (line.empty() || line.front() == L';') continue;
        if (line.size() > MaxLineLength) {
            LOG_WARN(L"INF line exceeds the length limit: " + path.wstring());
            continue;
        }

        if (!logical.empty()) logical += L' ';
        logical += line;
        if (!logical.empty() && logical.back() == L'\\') {
            logical.pop_back();
            continue; // continue onto the next line
        }
        flushLogical();
    }
    flushLogical();

    // ---- Pass 2: [Strings] and [Version] ------------------------------------
    for (const auto& section : sections) {
        if (LowerAscii(section.name) == L"strings") {
            for (const auto& line : section.lines) {
                const std::size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) continue;
                strings[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
            }
        } else if (LowerAscii(section.name) == L"version") {
            for (const auto& line : section.lines) {
                const std::size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) continue;
                const std::wstring key = LowerAscii(Trim(line.substr(0, eq)));
                const std::wstring value = Trim(line.substr(eq + 1));

                if (key == L"driverver") {
                    std::wstring date, version;
                    ParseDriverVer(value, date, version);
                    if (!date.empty()) {
                        entry.dateTime = ParseDate(date);
                        entry.dateEpoch = Hw::FileTimeToEpoch(entry.dateTime);
                    }
                    entry.versionStr = version;
                    entry.version = version;
                } else if (key == L"class") {
                    entry.className = value;
                } else if (key == L"classguid") {
                    entry.classGuid = value;
                } else if (key == L"catalogfile") {
                    entry.catalogFile = value;
                } else if (key == L"provider") {
                    entry.provider = ResolveStrings(value, strings);
                }
            }
        }
    }

    // ---- Pass 3: model sections --------------------------------------------
    const DWORD hostBuild = HostBuildNumber();

    // [Manufacturer] maps a vendor name to one or more model-section bases.
    std::vector<std::wstring> modelBases;
    {
        auto it = sectionIndex.find(L"manufacturer");
        if (it != sectionIndex.end()) {
            for (const auto& line : sections[it->second].lines) {
                const std::size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) continue;
                // RHS is "BaseSection[,Decoration[,Decoration...]]"; each
                // decoration yields a section named BaseSection.Decoration.
                const auto fields = SplitFields(std::wstring_view(line).substr(eq + 1));
                if (fields.empty() || fields[0].empty()) continue;
                const std::wstring& baseName = fields[0];
                modelBases.push_back(baseName);
                for (std::size_t i = 1; i < fields.size(); ++i) {
                    if (!fields[i].empty()) {
                        modelBases.push_back(baseName + L"." + fields[i]);
                    }
                }
            }
        }
    }

    if (modelBases.empty()) {
        // No [Manufacturer]: fall back to any section that looks decorated.
        for (const auto& section : sections) {
            const std::wstring lower = LowerAscii(section.name);
            if (lower.find(L".nt") != std::wstring::npos) {
                modelBases.push_back(section.name);
            }
        }
    }

    // For each base, pick the best-decorated section for this machine.
    // Different bases can resolve to the same section; emit it only once.
    std::unordered_set<std::size_t> emittedSections;
    for (const auto& base : modelBases) {
        const std::wstring baseLower = LowerAscii(base);
        std::size_t bestSection = std::size_t(-1);
        SectionMatch best;

        for (std::size_t i = 0; i < sections.size(); ++i) {
            const SectionMatch m = MatchModelSection(LowerAscii(sections[i].name),
                                                    baseLower, hostBuild);
            if (!m.matches || m.archScore <= 0) continue;

            const bool better =
                bestSection == std::size_t(-1) ||
                m.archScore > best.archScore ||
                (m.archScore == best.archScore &&
                 ((m.hasBuild && !best.hasBuild) ||
                  (m.hasBuild && best.hasBuild && m.build > best.build)));
            if (better) {
                bestSection = i;
                best = m;
            }
        }

        if (bestSection == std::size_t(-1)) continue;
        if (!emittedSections.insert(bestSection).second) continue;
        const Section& chosen = sections[bestSection];

        for (const auto& line : chosen.lines) {
            InfEntry::Model model;
            model.osBuild = static_cast<int>(best.hasBuild ? best.build : 0);
            if (!ParseModelLine(line, strings, chosen.name, model)) continue;

            for (const auto& hwid : model.hardwareIds) {
                InfEntry::HardwareMatch match;
                match.hardwareId = hwid;
                match.compatibleId = model.compatibleIds.empty()
                                         ? std::wstring()
                                         : model.compatibleIds.front();
                match.sourceSection = chosen.name;
                entry.hardwareMatches.push_back(std::move(match));
            }
            entry.models.push_back(std::move(model));
        }
    }

    if (entry.manufacturerSection.empty() && !modelBases.empty()) {
        entry.manufacturerSection = modelBases.front();
    }

    result.push_back(std::move(entry));
    return result;
}

InfEncoding DetectInfEncoding(std::filesystem::path path) {
    auto size = std::uint32_t{0};
    auto data = ReadFileBytes(path, &size);
    if (!data || size == 0) return InfEncoding::Ansi;
    return DetectEncoding(size, data.get());
}

} // namespace DriverNest::Packs
