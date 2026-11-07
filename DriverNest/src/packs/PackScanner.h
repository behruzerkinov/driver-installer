#pragma once
#include <windows.h>
#include <filesystem>
#include <vector>

namespace DriverNest::Packs {

// ---------------------------------------------------------------------------
// Walks a pack folder tree and finds all .inf and .cat files.
// ---------------------------------------------------------------------------
struct PackScanner {
    // Return absolute paths to all .inf files under the given root.
    [[nodiscard]] static std::vector<std::filesystem::path> FindInfFiles(
        const std::filesystem::path& root);

    // Return absolute paths to all .cat files under the given root.
    [[nodiscard]] static std::vector<std::filesystem::path> FindCatalogFiles(
        const std::filesystem::path& root);

    // Return true if a .cat file exists next to the given .inf file.
    [[nodiscard]] static bool HasCatalog(const std::filesystem::path& infPath);
};

} // namespace DriverNest::Packs
