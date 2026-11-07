# Testing

## Unit Tests

Run from the build directory:

```powershell
ctest --preset release-x64
```

Or run the test executable directly:

```powershell
.\out\release-x64\bin\DriverNestTests.exe
```

### Test Coverage

- **INF parser**: UTF-16 LE, UTF-8, ANSI, line continuation, `%strings%` substitution, model decoration selection for multiple build numbers.
- **Ranker**: Each scoring row in section 7.3 of the spec (exact match primary, exact match other, compatible ID only, class GUID bonus, newer date, newer version, signature bonus, unsigned penalty, manufacturer bonus, problem device bonus).
- **PackIndex**: Round trip save and load, incremental update when one file changes, corrupt header rejected.
- **Hash**: SHA-256 known vector ("abc" = `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`).
- **Path validation**: Rejects quotes and control characters.

## Manual Test Checklist

### Clean Install

- [ ] Install on Windows 10 22H2 (x64).
- [ ] Install on Windows 11 23H2 (x64).
- [ ] Install on Windows 11 24H2 (x64).
- [ ] Run without administrator — should show error and exit.
- [ ] Run as administrator — window should appear.

### Device Enumeration

- [ ] Device list matches Device Manager for the same machine.
- [ ] Problem devices show correct problem code.
- [ ] Missing drivers are identified.
- [ ] F5 refreshes the list.
- [ ] Search filters by name, hardware ID, and class.

### Driver Packs

- [ ] Set a pack folder in Settings.
- [ ] Index builds and shows driver count in status bar.
- [ ] Candidates appear for devices with matching hardware IDs.
- [ ] Ranking follows the scoring table in section 7.3.
- [ ] Unsigned candidates are hidden by default.
- [ ] Enable "Show unsigned" to see unsigned candidates.

### Installation

- [ ] Select a driver and click Install.
- [ ] Confirm dialog shows signer, version change, and restore point notice.
- [ ] Restore point is created before installation.
- [ ] Progress dialog shows per-driver status.
- [ ] After install, device shows green if problem code is 0 and version matches.
- [ ] Undo last batch restores the previous package.
- [ ] Clean old packages action lists unused packages.

### System Restore

- [ ] Create restore point successfully when System Protection is enabled.
- [ ] Show warning when System Protection is disabled.
- [ ] Allow user to continue after warning.

### Themes and Appearance

- [ ] Dark theme on Windows 11 with Mica backdrop.
- [ ] Light theme on Windows 11 with Mica backdrop.
- [ ] System theme follows Windows setting.
- [ ] Solid background on Windows 10 (no Mica).
- [ ] System accent color is used when set.
- [ ] Contrast ratio at least 4.5:1 for body text in both themes.

### DPI

- [ ] Window looks correct at 100% DPI.
- [ ] Window looks correct at 125% DPI.
- [ ] Window looks correct at 150% DPI.
- [ ] Window looks correct at 200% DPI.
- [ ] Window size and position are remembered.

### Accessibility

- [ ] All controls reachable by Tab in logical order.
- [ ] Narrator reads control labels correctly.
- [ ] Custom-drawn elements have AccessibleName set.

### Offline Operation

- [ ] Works fully offline with a local pack folder.
- [ ] No network activity when online is disabled (verify with firewall log).
- [ ] Online manifest URL can be set in Settings.
- [ ] Download progress shown with speed and cancel button.

### Edge Cases

- [ ] Corrupt index file is detected and rebuilt.
- [ ] Pack folder with 10,000 INF files can be indexed.
- [ ] INF file over 16 MB is rejected.
- [ ] INF file with line over 8192 characters is rejected.
- [ ] INF file without catalog is handled correctly.
- [ ] Two instances cannot run simultaneously.

### Crash Recovery

- [ ] Crash dump is written to `%LOCALAPPDATA%\DriverNest\crash\` on unhandled exception.
- [ ] Dialog appears with crash dump location.

## Running Tests Locally

The test script runs unit tests and checks the result:

```powershell
# Run all tests
ctest --preset release-x64

# Run a single test
.\out\release-x64\bin\DriverNestTests.exe --gtest_filter=RankerExactMatchPrimary
```

Note: The current test runner does not support `--gtest_filter`; all tests run by default.
