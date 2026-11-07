#include "Hash.h"
#include "Log.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <memory>
#include <vector>
#include <iomanip>
#include <sstream>

namespace DriverNest::Net {

using App::LOG_ERROR;
using App::LOG_INFO;
using App::LOG_WARN;

namespace {

// Convert a byte array to lowercase hex string.
std::string BytesToHex(const BYTE* data, size_t len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // namespace

std::string Sha256File(const std::wstring& path) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                                      nullptr, 0))) {
        LOG_ERROR(L"BCryptOpenAlgorithmProvider failed for SHA-256.");
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        LOG_ERROR(L"BCryptCreateHash failed.");
        return {};
    }

    // Read file in chunks
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    auto size = file.tellg();
    file.seekg(0);

    const size_t chunkSize = 64 * 1024; // 64 KB
    auto buf = std::make_unique<BYTE[]>(chunkSize);

    while (file) {
        file.read(reinterpret_cast<char*>(buf.get()), chunkSize);
        std::streamsize read = file.gcount();
        if (read > 0) {
            if (!BCRYPT_SUCCESS(BCryptHashData(hHash, buf.get(), static_cast<ULONG>(read), 0))) {
                BCryptDestroyHash(hHash);
                BCryptCloseAlgorithmProvider(hAlg, 0);
                return {};
            }
        }
    }

    // Finalize
    DWORD hashSize = 32; // SHA-256 = 256 bits = 32 bytes
    auto hash = std::make_unique<BYTE[]>(hashSize);
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash.get(), hashSize, 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BytesToHex(hash.get(), hashSize);
}

std::string Sha256Bytes(std::span<const BYTE> data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                                      nullptr, 0))) {
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(hHash, const_cast<PUCHAR>(data.data()),
                                       static_cast<ULONG>(data.size()), 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    DWORD hashSize = 32;
    auto hash = std::make_unique<BYTE[]>(hashSize);
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash.get(), hashSize, 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BytesToHex(hash.get(), hashSize);
}

std::string Sha256KnownVector() {
    // SHA-256 of "abc"
    const char* data = "abc";
    std::span<const BYTE> span(reinterpret_cast<const BYTE*>(data), 3);
    return Sha256Bytes(span);
}

} // namespace DriverNest::Net
