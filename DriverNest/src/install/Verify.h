#pragma once
#include <filesystem>
#include <string>

namespace DriverNest::Install {

// ---------------------------------------------------------------------------
// Signature verification and post-install verification.
// ---------------------------------------------------------------------------

// Verify that a catalog file is signed and the signature is valid.
[[nodiscard]] bool VerifyCatalogSignature(const std::filesystem::path& catalogPath);

// Verify an INF file is installed correctly by checking:
//  - The INF is in %WINDIR%\INF
//  - The matching .cat exists
//  - SetupDiGetDeviceInstallParams shows the driver is installed
[[nodiscard]] bool VerifyInstallation(const std::filesystem::path& installedInfPath);

// Get the signer name from a catalog file.
[[nodiscard]] std::wstring GetCatalogSigner(const std::filesystem::path& catalogPath);

} // namespace DriverNest::Install
