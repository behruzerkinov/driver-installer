# DriverNest v1.3.1

**Find it. Verify it. Install it.**

DriverNest is a free, portable Windows driver utility with an **Aerospace Deep Chassis** interface. It detects devices, ranks matching INF packages from a local driver pack, validates catalog signatures, and installs confirmed candidates.

- Single portable executable, zero third-party runtime dependencies.
- High-contrast graphite surfaces, cobalt accent, deterministic native GDI rendering.
- Local INF indexing, hardware ID candidate ranking, and catalog signature validation.
- Separate confirmation for each unsigned driver candidate.
- Automated CI/CD releases directly via GitHub Actions.

## Requirements

- Windows 10 (1809+) or Windows 11, x64
- Administrator privileges (required for driver installation)
- MSVC v143 (Visual Studio 2022 Build Tools) with Windows SDK 10.0.22621.0 or newer
- CMake 3.25+
- Ninja (optional, used by CMake presets)

## Building

```powershell
# Configure
cmake --preset release-x64

# Build
cmake --build --preset release-x64

# Run tests
ctest --preset release-x64

# Package (builds, tests, creates zip)
pwsh tools/package.ps1 -Version 1.0.0
```

The release executable is `out/release-x64/bin/DriverNest.exe`.

### Prerequisites

Install the required tools:

```powershell
# CMake
winget install Kitware.CMake

# Visual Studio Build Tools with C++ workload
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

## Usage

1. Run `DriverNest.exe` as Administrator.
2. Set a driver pack folder in **Settings** (a folder containing `.inf` and `.cat` files).
3. Click **Scan** to enumerate devices and find driver candidates.
4. Select devices and click **Install selected** to install drivers.
5. Confirm each selected package after reviewing its version and signature status.

### Driver Packs

A driver pack is a folder tree containing `.inf` files with matching `.cat` catalogs. Place your packs in a folder and set that folder as the pack root in Settings.

### Online Sources

Online catalog discovery is not available in this release. DriverNest only uses the local driver pack selected in Settings.

## What it does not do

- Back up or restore arbitrary driver files. Use Device Manager or Windows System Restore for recovery.
- Provide a GUI for editing INF files.
- Support Windows 7 or 8.
- Include a cloud account system.
- Collect any telemetry.
- Automatically install unsigned drivers.
- Bundle any third-party UI framework or decompressor.

## Security

- No network activity unless you enable online sources in Settings.
- Every downloaded package is hash-verified before use.
- Unsigned drivers require explicit per-driver confirmation.
- INF files are parsed with bounds checks; files over 16 MB or lines over 8192 characters are rejected.
- Only `pnputil.exe` and `7z.exe` (if configured) are spawned, always via full absolute paths under `%SystemRoot%\System32`.
- Settings and cache are stored under `%LOCALAPPDATA%\DriverNest\` with a restricted ACL.
- Built with `/GS /guard:cf /DYNAMICBASE /NXCOMPAT /CETCOMPAT` and `/HIGHENTROPYVA`.

## License

MIT License — see [LICENSE](LICENSE).

## Contributing

This is an original implementation. Do not copy code, artwork, icons, strings, or driver index formats from other driver tools.

## Documentation

- [Architecture](docs/architecture.md)
- [Design Decisions](docs/decisions.md)
- [Pack Format](docs/pack-format.md)
- [Security](docs/security.md)
- [Testing](docs/testing.md)

## Project URL

<repository URL set at build time via CMake variable DN_PROJECT_URL>
