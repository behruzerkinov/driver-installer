# Security

DriverNest is designed with security as a primary concern. These requirements are not optional.

## 1. No Network Activity by Default

- No network activity unless the user explicitly enables online sources in Settings.
- No telemetry, no update checks, no analytics of any kind.
- Every network connection is initiated by the user through the opt-in online manifest feature.
- Verify with a firewall log during testing: there should be zero outbound connections when online is disabled.

## 2. Download Verification

- Every downloaded package is hash-verified (SHA-256 via BCrypt) before it is touched.
- Mismatch deletes the downloaded file.
- The expected hash comes from the online manifest, which the user must trust.

## 3. Signature Verification

- Unsigned drivers are never installed without explicit per-driver confirmation.
- Catalog files are verified using `WinVerifyTrust` with `WINTRUST_ACTION_GENERIC_VERIFY_V2` and `CryptCATAdmin` APIs.
- Drivers without a catalog or with an invalid signature are hidden by default.
- The "Show unsigned" setting exposes them, but each still requires confirmation.

## 4. INF Parsing Safety

INF files are untrusted input. Parse with bounds checks on every buffer:

- Maximum INF file size: 16 MB.
- Maximum line length: 8192 characters.
- Files exceeding limits are rejected and logged, not parsed.
- Buffer overflows are prevented by using `std::wstring` and size-aware operations.

## 5. Process Execution

Only known executables are spawned:

- `pnputil.exe` (from `%SystemRoot%\System32\pnputil.exe`)
- `7z.exe` (if configured, from the configured path)

Paths are validated:
- Reject paths containing `"` (double quote).
- Reject paths containing control characters (char codes 0-31).
- Use full absolute paths, never relative paths.
- Never invoke through `cmd.exe` or other shells.

## 6. App Data Directory Security

All settings, cache, logs, and journals are stored under `%LOCALAPPDATA%\DriverNest\`.

- The directory is created with a restricted ACL.
- Only the current user and the Administrators group have access.
- No other users or system accounts can read or modify the data.

## 7. Build Hardening

Release builds are compiled with:

- `/GS` — Buffer security check (stack canaries).
- `/guard:cf` — Control Flow Guard.
- `/DYNAMICBASE` — ASLR.
- `/NXCOMPAT` — Data Execution Prevention (DEP).
- `/CETCOMPAT` — Control-flow Enforcement Technology (Intel CET).
- `/HIGHENTROPYVA` — High-entropy ASLR (64-bit).

Link with:

- `/MANIFEST:NO` (manifest embedded via resource file with elevation requirement).

## 8. Elevation Requirements

- The application requires administrator privileges via the manifest (`requireAdministrator`).
- If not elevated, show a message and exit. Do not attempt a runas relaunch loop.
- Check elevation with `CheckTokenMembership` against the Administrators SID at startup.

## 9. Single Instance

- Create a named mutex `Global\DriverNest.SingleInstance`.
- If it already exists, find the existing window via `FindWindow` on the registered class name, bring it forward, and exit.
- This prevents multiple instances from conflicting.

## 10. Crash Dumps

- `SetUnhandledExceptionFilter` writes a minidump to `%LOCALAPPDATA%\DriverNest\crash\` using `MiniDumpWriteDump`.
- Show a dialog to the user with the crash dump location.
- Do not send crash dumps anywhere; the user must report them manually.

## 11. Secure Coding Practices

- Use RAII for every handle: wrap `HANDLE`, `HKEY`, `HDEVINFO`, `SC_HANDLE`, and `CATADMIN` in move-only wrapper types.
- Use `std::wstring` and `std::filesystem::path` for paths.
- Convert to UTF-8 only at file and log boundaries.
- Validate all input from INF files, manifests, and user input.
- Log every Win32 failure with `GetLastError()` and `FormatMessageW` text.

## 12. Signing

- Sign the release executable with an Authenticode certificate if available.
- Use `signtool` with a timestamp server in `tools/package.ps1`.
- If no certificate is available, document it in the README and do not claim the binary is signed.
- The certificate should be stored securely, not in the repository.

## 13. Dependency Management

- No external dependencies beyond those named in the spec.
- If a dependency is truly needed, add it under `third_party/` with its license file and explain why in `docs/decisions.md`.
- The hand-written JSON parser in `net/Downloader.cpp` avoids the need for a JSON library.

## 14. Testing Security

- Test with System Protection disabled to verify the warning is shown.
- Test with high DPI at 200% to verify the UI remains usable.
- Test dark and light themes for contrast (minimum 4.5:1 for body text).
- Test with Narrator on to verify accessibility.
- Test on Windows 10 22H2, Windows 11 23H2, and 24H2.
