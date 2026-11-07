# DriverNest

[![Build and Release](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml)
[![Latest Release](https://img.shields.io/github/v/release/behruzerkinov/driver-installer?color=blue&label=release)](https://github.com/behruzerkinov/driver-installer/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20(x64)-informational)](https://github.com/behruzerkinov/driver-installer)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/behruzerkinov/driver-installer/blob/main/DriverNest/LICENSE)

> **Find it. Verify it. Install it.**  
> Free, lightweight, zero-dependency portable Windows driver management and deployment suite.

---

## Overview

**DriverNest** is a native Win32/C++20 utility engineered for hardware discovery, driver catalog matching, signature verification, and transactional installation. Designed around an industrial **Aerospace Deep Chassis** design system, DriverNest provides real-time hardware telemetry, per-device download and installation lifecycles, and rollback protection without third-party frameworks or runtime bloat.

---

## Key Features

* **High-Precision Industrial UI**:
  * Bespoke low-fatigue dark chassis palette (`#0F1115` base, `#181B22` deck, `#2F3646` borders, `#2563EB` cobalt accent).
  * Dual-font telemetry typography pairing `Segoe UI Variable` with monospace `Cascadia Code`.
  * Shimmer scanline skeleton loaders during device tree interrogation.
  * Draggable dual-pane splitter with real-time cursor feedback (`IDC_SIZEWE`) and grip pills.
  * Custom GDI-rendered checkboxes with micro-hover interactions and emerald ticks.
* **Granular Install Lifecycle Telemetry**:
  * Device entries report explicit real-time states: `Waiting`, `Downloading (XX% / MB)`, `Installing`, `Verifying`, `Installed`, `Pending Restart`, and `Failed`.
  * Inline sub-progress bars and status signal indicators directly inside the device list rows.
* **Session Install Journal & Smart Rescan**:
  * Thread-safe session journal tracks staged INF files, version transitions, and reboot requirements.
  * Prevents cyclic driver re-installation loops by recognizing staged packages awaiting reboot.
* **One-Click Batch Actions**:
  * Mandatory **"Select All" / "Deselect All"** toolbar action and `Ctrl+A` accelerator with dynamic counter badge.
  * Automated System Restore point creation before committing any batch installation.
* **Automated CI/CD Releases**:
  * Automated cloud builds via GitHub Actions on Windows runners.
  * Releases, packaged archives (`DriverNest-vX.Y.Z-windows-x64.zip`), and SHA-256 checksums are built and published automatically in the GitHub repository's **Releases** tab.

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
