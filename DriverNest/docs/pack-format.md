# Driver Pack Format

A "pack" is a folder tree containing `.inf` files with matching `.cat` catalogs. DriverNest uses these to find driver candidates for installed devices.

## v1: Uncompressed Folder Trees Only

Compressed archives (.7z, .zip) are not supported in v1. Users must extract archives themselves or use an external `7z.exe` found on PATH. DriverNest does not bundle a decompressor.

## Folder Structure

A pack is any folder containing `.inf` files. The structure is flat or nested; DriverNest walks the entire tree.

```
PackRoot/
  drivers/
    network/
      netadapter.inf
      netadapter.cat
    storage/
      ssd.inf
      ssd.cat
    audio/
      audio.inf
      audio.cat
```

Each `.inf` file should have a corresponding `.cat` catalog file with the same base name in the same directory. The catalog file is used for signature verification.

## INF File Requirements

DriverNest's INF parser understands:

- UTF-16 LE with BOM
- UTF-8 with BOM
- ANSI (Windows-1252) without BOM
- Comments starting with `;`
- Sections in `[brackets]`
- Key-value lines with quoted strings
- `%StringKey%` references resolved from `[Strings]`
- Line continuation with trailing `\`
- `[Manufacturer]` section with model sections
- `[Version]` section with `DriverVer`, `Class`, `ClassGuid`, `CatalogFile`, `Provider`
- Hardware IDs from model lines

### Limits

- Maximum INF file size: 16 MB
- Maximum line length: 8192 characters
- Files exceeding these limits are rejected and logged.

## Index File (index.dnidx)

DriverNest builds a binary index for fast lookup. The index is stored as `index.dnidx` in `%LOCALAPPDATA%\DriverNest\`.

### Format Version 1

```
Header:
  magic:      4 bytes  "DNIX"
  version:    uint32   1
  recordCount: uint32   number of records

Root path:
  null-terminated UTF-8 string (absolute path of the pack root)

Records (repeated recordCount times):
  hardwareIdLen:   uint32   byte length of UTF-8 hardware ID
  hardwareId:      bytes    hardware ID (lowercase for lookup)
  infRelPathLen:   uint32   byte length of relative INF path
  infRelPath:      bytes    relative path from pack root
  version:         uint64   packed version (major<<48 | minor<<32 | build<<16 | revision)
  dateEpoch:       int64    FILETIME as 100-nanosecond ticks since 1601-01-01
  fileMtime:       int64    FILETIME of the INF file
```

### Hardware ID Lookup

A hash map is built from lowercase hardware ID to vector of record indexes. Lookup is case-insensitive.

### Rebuilding the Index

The index is rebuilt by:
1. Scanning the pack root recursively for `.inf` files.
2. Parsing each INF file.
3. Extracting hardware matches.
4. Creating one record per hardware match.
5. Saving the index to disk.

### Incremental Updates

When the pack root changes, DriverNest compares file mtimes and updates only changed records. In v1, this falls back to full rebuild if the incremental path cannot guarantee correctness.

## Catalog Files

Catalog files (.cat) are used for signature verification. DriverNest uses:
- `CryptCATAdminAcquireContext`
- `CryptCATAdminCalcHashFromFileHandle`
- `CryptCATAdminEnumCatalogFromHash`
- `WinVerifyTrust` with `WINTRUST_ACTION_GENERIC_VERIFY_V2`

Catalog verification results are used in ranking (signed bonus/penalty).

## Online Manifest Format

When online sources are enabled, DriverNest reads a JSON manifest from a URL set in Settings. The default is empty (offline only).

```json
{
  "hardwareIds": ["PCI\\VEN_8086&DEV_1234", "USB\\VID_1234&PID_5678"],
  "url": "https://example.com/drivers/package.zip",
  "sha256": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
  "size": 12345678,
  "signer": "Microsoft Windows"
}
```

### Fields

- `hardwareIds`: Array of hardware IDs this package supports.
- `url`: URL to download the driver package.
- `sha256`: Expected SHA-256 hash of the downloaded file (lowercase hex).
- `size`: Expected file size in bytes.
- `signer`: Expected signer name.

### Download and Verification

1. Download to `%LOCALAPPDATA%\DriverNest\cache\` with `.part` suffix.
2. Verify SHA-256 after download.
3. Rename to final path on success.
4. Extract only if the archive is a supported format and the external extractor is available.

## Creating a Pack

1. Gather INF and CAT files from Windows Driver Kit or hardware vendor.
2. Place them in a folder tree.
3. Set that folder as the pack root in DriverNest Settings.
4. Click Scan to index and find candidates.
