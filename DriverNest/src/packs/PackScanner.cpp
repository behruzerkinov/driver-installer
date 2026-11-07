#include "PackScanner.h"
#include <algorithm>
#include <system_error>
#include <stack>

namespace DriverNest::Packs {

namespace {

// Recursive walk using FindFirstFileExW with FIND_FIRST_EX_LARGE_FETCH.
// This is more efficient for large directory trees.
void WalkDirectory(const std::filesystem::path& dir,
                   std::vector<std::filesystem::path>& out,
                   const wchar_t* extension) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) return;

    // Use Windows API for better performance
    std::wstring searchPath = dir.wstring() + L"\\*";
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoStandard,
                                     &findData, FindExSearchNameMatch, nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::filesystem::path fullPath = dir / name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            WalkDirectory(fullPath, out, extension);
        } else {
            // Check extension
            if (extension != nullptr) {
                std::wstring ext = fullPath.extension().c_str();
                if (_wcsicmp(ext.c_str(), extension) == 0) {
                    out.push_back(fullPath);
                }
            } else {
                out.push_back(fullPath);
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

} // namespace

std::vector<std::filesystem::path> PackScanner::FindInfFiles(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path> result;
    WalkDirectory(root, result, L".inf");
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::filesystem::path> PackScanner::FindCatalogFiles(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path> result;
    WalkDirectory(root, result, L".cat");
    std::sort(result.begin(), result.end());
    return result;
}

bool PackScanner::HasCatalog(const std::filesystem::path& infPath) {
    std::filesystem::path catPath = infPath;
    catPath.replace_extension(L".cat");
    return std::filesystem::exists(catPath);
}

} // namespace DriverNest::Packs
