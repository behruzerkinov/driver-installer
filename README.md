# DriverNest v1.3.0

[![Build and Release](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/behruzerkinov/driver-installer/actions/workflows/build-and-release.yml)
[![Latest Release](https://img.shields.io/github/v/release/behruzerkinov/driver-installer?color=blue&label=release)](https://github.com/behruzerkinov/driver-installer/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20(x64)-informational)](https://github.com/behruzerkinov/driver-installer)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/behruzerkinov/driver-installer/blob/main/DriverNest/LICENSE)

> **Find it. Verify it. Install it.**  
> Free, lightweight, zero-dependency portable Windows driver management and deployment suite.

---

## Overview

**DriverNest** is a native Win32/C++20 utility engineered for hardware discovery, driver catalog matching, signature verification, and transactional installation. Redesigned with an authentic **Unigram Windows Blurry Aero & Acrylic UI**, DriverNest combines hardware-accelerated DWM glass blur with translucent cards, rich squircle category avatars, real-time hardware telemetry, per-device download and installation lifecycles, and rollback protection without third-party frameworks or runtime bloat.

---

## Key Features

* **Unigram-Inspired Windows Blurry Aero & Acrylic UI**:
  * Hardware-accelerated Acrylic / Aero Glass blur backdrop via DWM (`DWMWA_SYSTEMBACKDROP_TYPE = 3` on Windows 11 and `SetWindowCompositionAttribute` on Windows 10).
  * Full-bleed borderless glass extension (`DwmExtendFrameIntoClientArea`).
  * Translucent glass card containers with specular hairline borders and top-edge light catch lines (`Theme::DrawGlassCard`).
  * Telegram/Unigram chat-style master device list with 58px card rows, vibrant squircle category avatars (Cobalt, Purple, Emerald, Amber, Cyan), and two-line typography hierarchy.
  * Right-aligned floating status pill badges and Unigram-style active card selection pills with vertical accent indicator bars.
* **Granular Install Lifecycle Telemetry**:
  * Device entries report explicit real-time states: `Waiting`, `Downloading (XX% / MB)`, `Installing`, `Verifying`, `Installed`, `Pending Restart`, and `Failed`.
  * Inline sub-progress bars and status signal indicators directly inside the device list rows.
* **Hardware Inspector Deck (Right Pane)**:
  * Hero device profile card with large glowing squircle avatar, manufacturer info, and class GUID.
  * Side-by-side comparative telemetry bento grid comparing currently installed store drivers with staged candidate packages.
  * Hardware identification deck with instant "Copy ID" action.
* **Session Install Journal & Smart Rescan**:
  * Thread-safe session journal tracks staged INF files, version transitions, and reboot requirements.
  * Prevents cyclic driver re-installation loops by recognizing staged packages awaiting reboot.
* **One-Click Batch Actions**:
  * Mandatory **"Select All" / "Deselect All"** toolbar action and `Ctrl+A` accelerator with dynamic counter badge.
  * Automated System Restore point creation before committing any batch installation.
* **Automated CI/CD Releases**:
  * Automated cloud builds via GitHub Actions on Windows runners (`windows-2022`).
  * Releases, packaged archives (`DriverNest-1.3.0-windows-x64.zip`), and SHA-256 checksums are built and published automatically in the GitHub repository's **Releases** tab.

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
