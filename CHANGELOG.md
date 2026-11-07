# Changelog

All notable changes to DriverNest are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.3.1] - 2026-11-08

### Fixed
- Restored saved theme preference on startup and kept custom control fonts valid across theme changes.
- Replaced uninitialized alpha compositing and per-card DIB allocation with predictable solid GDI surfaces.
- Removed scan startup delay, blocked overlapping scans, and moved install progress updates onto the UI thread.
- Connected local pack indexing and signature-aware candidate ranking to the device list and install flow.
- Added working Settings actions for pack folder, theme, and unsigned candidates.
- Replaced the recursive, nonfunctional Undo action with accurate recovery guidance.
- Corrected scan status messages so missing packages are not reported as an up-to-date system.

### Changed
- Matched the dark palette to Aerospace Deep Chassis tokens and cobalt accent.
- Reworked driver candidate installation to use the selected INF and report actual failures.
- Removed the inaccurate automatic restore-point claim from the install confirmation and release notes.
- Updated release version to 1.3.1.

## [1.3.0] - 2026-11-08

### Added
- **Unigram Windows Native Blurry Aero & Acrylic UI**:
  - Hardware-accelerated Acrylic frosted glass blur backdrop via DWM (`DWMWA_SYSTEMBACKDROP_TYPE = 3` on Windows 11 22621+).
  - Legacy Windows 10 Acrylic support via `SetWindowCompositionAttribute` (`ACCENT_ENABLE_ACRYLICBLURBEHIND`).
  - Full-bleed borderless glass extension into the client area via `DwmExtendFrameIntoClientArea`.
  - Translucent glass cards with specular highlight borders and soft rounded corners (`Theme::DrawGlassCard`).
- **Telegram/Unigram Chat-Style Master Device Feed**:
  - Expanded 58px card row height with generous breathing room and information hierarchy.
  - Vibrant squircle avatars with category-specific glows (Cobalt for Display, Purple for Audio, Emerald for Network, Amber for USB, Cyan for System).
  - Two-line typography pairing bold device descriptions with monospace version and INF telemetry.
  - Floating Unigram-style status pill badges with pulsing signal indicators.
  - Unigram selection pill with 3px vertical accent indicator bar on the active card.
- **Translucent Hardware Inspector Deck**:
  - Unigram profile hero card with large glowing squircle avatar, manufacturer, and class GUID.
  - Two-column comparative telemetry bento grid (`Installed Store` vs `Staged Candidate`).
  - Specular frosted glass action pill buttons ("Install Driver", "Copy ID").
- **Aero Command Deck & Floating Toolbar**:
  - Integrated titlebar with glowing app squircle emblem, frosted version pill (`v1.3.0`), and telemetry subtitle.
  - Owner-drawn circular frosted glass buttons for theme switching and settings.
  - Unigram aero pill buttons for hardware scanning, batch selection, and installation.

---

## [1.2.0] - 2026-11-08

### Added
- **Automated GitHub Actions CI/CD Pipeline**:
  - Continuous integration workflow in `.github/workflows/build-and-release.yml`.
  - Automated building on Windows runners with MSBuild and CMake C++20.
  - Automated execution of test suites (`DriverNestTests.exe`).
  - Automated packaging of standalone release archives (`DriverNest-v1.2.0-windows-x64.zip`) with SHA-256 checksums.
  - Automatic creation and publishing of GitHub Releases directly on push or tag.
- **Design Intelligence & Skill Integration**:
  - Integrated `nextlevelbuilder/ui-ux-pro-max`, `sickn33/scroll-experience`, and `frontend-design`.
  - Global Gemini / Antigravity rule directives enforcing continuous UI/UX quality across all future development.
- **Dynamic Version Synchronization**:
  - Application title bar, custom header version pill, loggers, and telemetry panes dynamically reflect active semantic release versions.

---

## [1.1.0] - 2026-11-08

### Added
- **Aerospace Deep Chassis Design System**:
  - Calibrated low-fatigue dark palette (`#0F1115` base, `#181B22` deck, `#212631` raised, `#2F3646` grid, `#2563EB` cobalt accent).
  - High-clarity typography hierarchy using `Segoe UI Variable` paired with `Cascadia Code` telemetry font.
- **Interactive Draggable Splitter**:
  - Dynamic dual-pane divider with real-time cursor feedback (`IDC_SIZEWE`), hover grip pill, and smooth mouse tracking.
- **Scanline Shimmer Skeletons**:
  - Animated multi-row pulse skeletons during hardware and device tree scans.
- **Mandatory "Select All" Controls**:
  - Added "Select All" / "Deselect All" toolbar action (`ID_APP_SELECT_ALL`), bound to hotkey `Ctrl+A`.
  - Live selection counter pill badge (`Install Selected (N)`).
- **Granular Per-Device Install Lifecycle**:
  - Real-time device state machine: `Waiting to Download`, `Downloading (XX% / X.X MB)`, `Installing`, `Verifying`, `Installed`, `Reboot Required`, and `Failed`.
  - In-row micro progress bars and color-coded telemetry status signals.
- **Session Install Journal & Smart Rescan**:
  - Thread-safe `InstallHistory` journal tracks staged packages, driver versions, and reboot prerequisites across user sessions.
  - Smart rescan recognition: drivers awaiting reboot are recognized as `PendingRestart`, preventing duplicate re-install offers.
- **Hardware Inspector Deck**:
  - 2-column comparative telemetry matrix (`Installed` vs `Staged candidate`).
  - Clickable "Copy ID" button for fast hardware identifier clipboard capture.

---

## [1.0.0] - 2026-11-07

### Added
- Initial standalone release of DriverNest portable Windows driver installer.
- High-speed device enumeration using native SetupAPI.
- Driver staging and injection engine using pnputil and DISM.
- System Restore point integration and automated rollback protection.
- Portable, zero-dependency native Win32/C++20 architecture.
