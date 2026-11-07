# Design Decisions

## Technology Stack

### Why Win32 and not WinUI 3

WinUI 3 requires the Windows App Runtime and produces a much larger deployment. Win32 with Common Controls 6 and DWM attributes gives the native look at a fraction of the size. Target is under 3 MB for the executable alone.

### Why C++20 and not C#/WinForms

C++20 with static CRT gives us a single portable executable with no runtime to install. C# with .NET 8 self-contained trimmed is a viable alternative but would be larger and require the .NET runtime. If Win32 blocks a requirement, C# with WinForms (.NET 8, self-contained, trimmed) is the documented fallback.

### Why no third-party UI framework

DriverNest targets a single portable executable under 3 MB. Third-party UI frameworks would add significant size. Win32 with Common Controls 6, DWM attributes, and Segoe Fluent Icons glyph references provide the native Windows 10/11 look.

## UI Decisions

### Mica on Windows 11, solid on Windows 10

- Windows 11 build 22000+: Use `DWMWA_SYSTEMBACKDROP_TYPE` with Mica (value 2).
- Windows 10: Solid surface color via `WM_ERASEBKGND`.
- Acrylic is not used per spec.

### Dark mode detection

- Read `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`.
- Follow system unless user overrides in settings.
- `DWMWA_USE_IMMERSIVE_DARK_MODE` applied to windows and dialogs on Windows 10 1809+.

### Segoe Fluent Icons

- Referenced by Unicode code point, never bundled as a font file.
- Used for glyphs in buttons and status indicators.

## Driver Pack Decisions

### Custom INF parser instead of SetupInstallFromInfSection

The custom parser keeps the scan fast and never touches the registry. It handles UTF-16 LE/BE, UTF-8, and ANSI (Windows-1252) input with BOM detection.

### Uncompressed pack folders only in v1

Compressed archives (.7z, .zip) are extracted by the user or by an optional external `7z.exe` found on PATH. No decompressor is bundled.

### Index format

Binary format with "DNIX" magic, format version 1. Records store lowercase hardware ID (UTF-8), relative INF path, packed version (uint64), date epoch (int64 FILETIME), and file mtime. Hash map for hardware ID lookup built on load.

## Installation Decisions

### pnputil as preferred install method

`pnputil.exe /add-driver ... /install` is the standard Windows tool for driver package installation. Run as a child process with `CREATE_NO_WINDOW`, capture stdout/stderr.

### UpdateDriverForPlugAndPlayDevices as fallback

For stubborn devices, `UpdateDriverForPlugAndPlayDevicesW` from `newdev.dll` is loaded at runtime. `INSTALLFLAG_FORCEIF` only when user checks "Force install".

### System Restore point before install

`SRSetRestorePointW` from `srclient.dll` is loaded at runtime, not linked statically, so the app still starts if the DLL is missing. Type `DEVICE_DRIVER_INSTALL` (10).

## Security Decisions

### Online sources off by default

No network activity unless the user explicitly enables online sources in Settings. No telemetry, no update checks.

### Hash verification of all downloads

SHA-256 computed via BCrypt. Mismatch deletes the file.

### Unsigned drivers require explicit confirmation

Each unsigned driver requires a per-driver confirmation. Unsigned drivers are hidden by default and shown only when "Show unsigned" is enabled.

### INF parsing limits

- File size: 16 MB maximum.
- Line length: 8192 characters maximum.
- Files exceeding limits are rejected and logged.

### Only known executables spawned

Only `pnputil.exe` and `7z.exe` (if configured) are spawned. Always via full absolute paths under `%SystemRoot%\System32` or the configured path. Never through `cmd.exe`.

### Path validation

Paths with `"` or control characters are rejected before being passed to child processes.

### Restricted ACL on app data

`%LOCALAPPDATA%\DriverNest\` created with ACL allowing only current user and Administrators.

## Trade-offs

### Device enumeration uses full scan on each refresh

For simplicity and correctness, each refresh performs a full SetupAPI enumeration. For a typical laptop (~80 devices) this completes in under 300 ms. Incremental refresh could be added later but was not required for v1.

### Index rebuild is full, not incremental in v1

The incremental update path exists but currently falls back to full rebuild for correctness. Incremental updates by mtime comparison will be refined in a later phase.

### Signature check is live, not cached in v1

Catalog signature is verified live during ranking. Caching per catalog path and mtime is planned but not implemented in the initial version.

### No support for Windows 7/8

The application target is Windows 10 (1809+) and Windows 11. Windows 7 and 8 are explicitly out of scope.
