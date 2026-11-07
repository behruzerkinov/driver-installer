# DriverNest Architecture

## Overview

DriverNest is a single-file portable Windows application written in C++20 using the Win32 API. It uses no third-party UI frameworks, no external DLLs (except system libraries), and links the CRT statically.

## Component Layers

```
main.cpp
  └── App.cpp/h          Application bootstrap, elevation, single instance, message loop
       ├── Settings.cpp/h  INI-based settings, app data directory with ACL
       └── Log.cpp/h       Rotating text log, 1 MB cap, 3 files

  hw/                    Hardware enumeration
       ├── DeviceEnum.cpp/h  SetupAPI device enumeration, CFGMGR problem codes
       └── DriverState.cpp/h  Driver state classification, version comparison

  packs/                 Driver pack indexing and ranking
       ├── InfParser.cpp/h   INF file parser (UTF-16/UTF-8/ANSI, %strings%, line continuation)
       ├── PackIndex.cpp/h   Binary index with DNIX magic, mmap, hash lookup
       ├── PackScanner.cpp/h Folder tree walker for .inf and .cat files
       └── Ranker.cpp/h      Candidate scoring per section 7.3

  install/               Driver installation
       ├── Installer.cpp/h   pnputil/SetupAPI installation, batch install, rollback
       ├── RestorePoint.cpp/h System Restore API via srclient.dll
       └── Verify.cpp/h      Signature verification, post-install check

  net/                   Online downloads (opt-in)
       ├── Downloader.cpp/h  WinHTTP with TLS 1.2, progress, cancel, hash verify
       └── Hash.cpp/h        SHA-256 via BCrypt

  ui/                    User interface
       ├── MainWindow.cpp/h  Main window with header, toolbar, split body, status bar
       ├── DeviceList.cpp/h  ListView wrapper with groups, checkboxes, custom draw
       ├── DetailsPane.cpp/h Device details and candidate display
       ├── Theme.cpp/h       Palette, dark/light detection, system accent, DWM dark mode
       ├── Backdrop.cpp/h    Mica on Win11, solid fallback on Win10
       ├── Dialogs.cpp/h     Confirm, settings, about, progress dialogs
       └── Strings.h         All user-facing English strings
```

## Threading Model

- **UI Thread**: Owns all windows and the device list model. Processes messages and updates UI.
- **Worker Thread Pool**: Uses `std::jthread` with a work queue. Runs scans, index builds, downloads, and installs.
- **Communication**: Results return to UI via `PostMessageW` with heap-allocated payloads. No `SendMessage` from workers to avoid deadlocks.
- **Cancellation**: `std::stop_token` passed to each job.

## Build System

- CMake 3.25+ with presets for `release-x64` and `debug-x64`.
- MSVC v143 (Visual Studio 2022 Build Tools).
- Static CRT (`/MT`), LTO in release (`/GL` + `/LTCG`).
- Security flags: `/GS /guard:cf /DYNAMICBASE /NXCOMPAT /CETCOMPAT /HIGHENTROPYVA`.

## Application Manifest

- `requireAdministrator` execution level.
- Common Controls 6 via manifest dependency.
- PerMonitorV2 DPI awareness.
- longPathAware.
- Supported OS: Windows 10 and Windows 11 GUIDs.

## Device Enumeration

Uses SetupAPI to enumerate all present devices:

1. `SetupDiGetClassDevsW` with `DIGCF_ALLCLASSES | DIGCF_PRESENT`.
2. For each device, read hardware IDs, compatible IDs, description, manufacturer, class GUID, driver key.
3. Get problem code via `CM_Get_DevNode_Status`.
4. Read installed driver version and date from registry.
5. Mark as Unsigned when signature check fails.

## Driver Pack Indexing

- INF files are parsed by a custom line-oriented parser (no SetupInstallFromInfSection).
- Index is stored in a binary file `index.dnidx` with magic "DNIX", format version 1.
- Hardware ID lookup uses a hash map (lowercase hardware ID → record indexes).
- Index is rebuilt or incrementally updated based on file mtimes.

## Installation Flow

1. Create System Restore point (via `SRSetRestorePointW` from `srclient.dll`).
2. Install each selected driver via `pnputil.exe /add-driver ... /install`.
3. Verify installation by checking INF in `%WINDIR%\INF` and signature.
4. Re-enumerate device and check problem code.
5. Record journal entry for undo.

## Rollback

- Journal stored as JSON lines in `%LOCALAPPDATA%\DriverNest\journal.log`.
- Undo re-installs previous INF via pnputil.
- "Clean old packages" uses `pnputil /delete-driver` for unused packages.

## Security

- No network by default; opt-in online sources only.
- Hash verification of all downloads.
- No execution of arbitrary programs from packs.
- Path validation rejects quotes and control characters.
- Restricted ACL on app data directory.

## Performance Targets

| Metric | Target |
|--------|--------|
| Cold start to window visible | < 400 ms |
| Device enumeration (80 devices) | < 300 ms |
| Index load (50,000 records) | < 150 ms |
| Full index rebuild (20 GB pack, 8 cores) | < 3 minutes |
| Binary size | < 3 MB |

## Testing

Unit tests use a minimal in-tree test runner (`tests/` with `TEST(name)` macro and `main`). No external test framework.

Required tests cover:
- INF parser: UTF-16, UTF-8, ANSI, line continuation, %strings% substitution, model decoration.
- Ranker: each scoring row in section 7.3.
- Index: round trip save/load, incremental update, corrupt header rejection.
- Hash: SHA-256 known vector.
- Path validation: rejects quotes and control characters.
