#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <span>
#include <cstddef>
#include <windows.h>

namespace DriverNest::Net {

// ---------------------------------------------------------------------------
// SHA-256 hashing via BCrypt.
// ---------------------------------------------------------------------------

// Compute SHA-256 of a file. Returns hex string (lowercase, 64 chars).
[[nodiscard]] std::string Sha256File(const std::wstring& path);

// Compute SHA-256 of a byte buffer.
[[nodiscard]] std::string Sha256Bytes(std::span<const BYTE> data);

// Known test vector for "abc":
// ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
[[nodiscard]] std::string Sha256KnownVector();

} // namespace DriverNest::Net
