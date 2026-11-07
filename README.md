# DriverNest v1.3.1

[![Build and Release](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml)
[![Latest Release](https://img.shields.io/github/v/release/behruzerkinov/driver-installer?color=blue&label=release)](https://github.com/behruzerkinov/driver-installer/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20(x64)-informational)](https://github.com/behruzerkinov/driver-installer)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/behruzerkinov/driver-installer/blob/main/DriverNest/LICENSE)

> **Find it. Verify it. Install it.**  
> Free, lightweight, zero-dependency portable Windows driver management and deployment suite.

---

## Overview

**DriverNest** is a native Win32/C++20 utility for hardware discovery and matching verified driver packages from a local driver pack. Its Aerospace Deep Chassis interface uses a high-contrast graphite palette, stable cobalt accent, compact device telemetry, and native GDI surfaces. The portable build has no third-party runtime dependencies.

---

## Key Features

* **Aerospace Deep Chassis UI**:
  * Dark graphite surfaces (`#0F1115`, `#181B22`, `#212631`) with cobalt accent (`#2563EB`).
  * Deterministic, opaque GDI rendering with compact telemetry rows and clear focus/selection states.
  * System, light, and dark theme modes with persisted settings.
* **Local Driver Pack Matching**:
  * Scans the configured pack folder, indexes INF packages, ranks hardware ID matches, and checks catalog signatures.
  * Unsigned candidates remain hidden unless enabled; each selected unsigned package gets a separate confirmation.
  * Installation progress is marshalled back to the UI thread and reports actual Windows install errors.
* **Hardware Inspector Deck (Right Pane)**:
  * Hero device profile card with large glowing squircle avatar, manufacturer info, and class GUID.
  * Side-by-side comparative telemetry bento grid comparing currently installed store drivers with staged candidate packages.
  * Hardware identification deck with instant "Copy ID" action.
* **Session Install Journal & Recovery**:
  * Tracks packages staged during the current session and flags devices awaiting restart.
  * Provides Windows recovery guidance; it does not claim to restore a previous driver package.
* **Batch Actions**:
  * Mandatory **"Select All" / "Deselect All"** toolbar action and `Ctrl+A` accelerator with dynamic counter badge.
  * Per-driver confirmation for unsigned packages and visible package-version review before installation.
* **Automated CI/CD Releases**:
  * Automated cloud builds via GitHub Actions on Windows runners (`windows-2022`).
  * Releases, packaged archives (`DriverNest-1.3.1-windows-x64.zip`), and SHA-256 checksums are built and published automatically in the GitHub repository's **Releases** tab.

---

## Automated Releases via GitHub Actions

DriverNest is configured to build and publish automatically through GitHub Actions. You do not need to compile binaries locally to distribute them:

* **Workflow**: [`.github/workflows/build-and-release.yml`](.github/workflows/build-and-release.yml)
* **Triggers**:
  * Pushes to `main` branch
  * Version release tags (`v*`, e.g. `v1.2.0`)
  * Manual triggers via the **Actions** tab in GitHub (`workflow_dispatch`)
* **Outputs**:
  * Standalone portable zip: `DriverNest-vX.Y.Z-windows-x64.zip`
  * Cryptographic checksum: `DriverNest-vX.Y.Z-windows-x64.zip.sha256`
  * Formatted changelog release notes directly in the GitHub Releases tab.

---

## Local Building & Testing (Optional)

### Prerequisites

* Windows 10 (1809+) or Windows 11 (x64)
* Visual Studio 2022 Build Tools (with C++ workload and Windows SDK 10.0.22621+)
* CMake 3.25+

```powershell
# Configure CMake
cmake -B build -S DriverNest -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# Compile solution
cmake --build build --config Release --parallel

# Execute unit test suite
ctest --test-dir build -C Release --output-on-failure
```

---

## Repository Structure

```
driver-installer/
├── .github/
│   └── workflows/
│       └── build-and-release.yml      # Automated CI/CD build & release pipeline
├── .agents/
│   ├── rules/                         # Workspace AI & UX standards
│   └── skills/                        # Integrated design and interaction skills
├── DriverNest/
│   ├── src/
│   │   ├── app/                       # Application entry point & lifecycle
│   │   ├── hw/                        # SetupAPI hardware scanning & device nodes
│   │   ├── install/                   # Driver staging, injection & InstallHistory
│   │   ├── rollback/                  # System Restore point & snapshot management
│   │   └── ui/                        # Custom Win32 GDI controls, Theme, Splitter
│   ├── tests/                         # Unit tests (DeviceList, InstallHistory, etc.)
│   └── CMakeLists.txt                 # Master CMake configuration (v1.2.0)
├── CHANGELOG.md                       # Comprehensive release notes
└── README.md                          # Repository documentation
```

---

## License

DriverNest is licensed under the [MIT License](DriverNest/LICENSE).
