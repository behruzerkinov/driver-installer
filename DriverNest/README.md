# DriverNest v1.4.0

**Find it. Verify it. Install it.**

DriverNest is a free, portable Windows driver utility with a dark, information-dense interface. It detects devices, ranks matching INF packages from a local driver pack, validates catalog signatures, and installs confirmed local candidates.

- Single portable executable, zero third-party runtime dependencies.
- Dark graphite surfaces with four accent colors and smooth accent transitions.
- Local INF indexing, hardware ID candidate ranking, and catalog signature validation.
- On-demand online hardware-ID lookup through WinDriverIndex, with OEM/Microsoft download links.
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

### Online Index

Click **Search online** to query [WinDriverIndex](https://github.com/dtucny/WinDriverIndex), a public open-source index. Its published metadata is CC BY 4.0; DriverNest displays a matching family version and opens the corresponding OEM or Microsoft Catalog source. Index coverage varies by hardware family, and its newest listing is not a quality or compatibility guarantee. Confirm the exact device model and supported Windows version on the vendor page before downloading. Online packages are opened from their source and are not installed by DriverNest; use a downloaded or extracted local INF pack for installation.

## What it does not do

- Back up or restore arbitrary driver files. Use Device Manager or Windows System Restore for recovery.
- Provide a GUI for editing INF files.
- Support Windows 7 or 8.
- Include a cloud account system.
- Collect any telemetry.
- Automatically install unsigned drivers.
- Bundle any third-party UI framework or decompressor.

## Security

- No network activity until you choose **Search online**.
- Online results are metadata and source links; DriverNest does not silently download or install them.
- Unsigned drivers require explicit per-driver confirmation.
- INF files are parsed with bounds checks; files over 16 MB or lines over 8192 characters are rejected.
- Only `pnputil.exe` and `7z.exe` (if configured) are spawned, always via full absolute paths under `%SystemRoot%\System32`.
- Settings and cache are stored under `%LOCALAPPDATA%\DriverNest\` with a restricted ACL.
- Built with `/GS /guard:cf /DYNAMICBASE /NXCOMPAT /CETCOMPAT` and `/HIGHENTROPYVA`.

## License

MIT License — see [LICENSE](LICENSE).

## Contributing

This is an original implementation. Online metadata is provided by WinDriverIndex under CC BY 4.0; its code is MIT licensed. Driver package rights remain with their OEM publishers.

## Documentation

- [Architecture](docs/architecture.md)
- [Design Decisions](docs/decisions.md)
- [Pack Format](docs/pack-format.md)
- [Security](docs/security.md)
- [Testing](docs/testing.md)

## Project URL

<repository URL set at build time via CMake variable DN_PROJECT_URL>
