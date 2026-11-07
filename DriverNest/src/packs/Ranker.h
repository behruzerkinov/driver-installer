#pragma once
#include "DeviceInfo.h"
#include "PackIndex.h"
#include <vector>
#include <string>
#include <cstdint>

namespace DriverNest::Packs {

// ---------------------------------------------------------------------------
// Rank candidate driver packages for a device.
//
// Scoring table (section 7.3 of spec):
//  - Exact match of most specific hardware ID (position 0): 1000
//  - Exact match of another hardware ID: 800 - 20 * index
//  - Compatible ID match only: 400
//  - Class GUID matches device class: +50
//  - Driver date newer than installed date: +30
//  - Version greater than installed version: +40
//  - Catalog present and signature valid: +100
//  - Signature invalid or missing catalog: -500 (hide by default)
//  - Provider matches device manufacturer: +20
//  - Device has problem code: +25
// ---------------------------------------------------------------------------
struct Ranker {
    struct Candidate {
        size_t recordIndex;    // index into PackIndex::records
        int score = 0;
        std::vector<std::wstring> reasonTags;
        std::wstring hardwareIdMatched;
        bool signedOK = false;  // signature valid
        bool catalogPresent = false;
    };

    // Rank candidates for a device from the given index.
    // Returns top candidate and up to two alternatives.
    static void Rank(const Hw::Device& device,
                     const PackIndex& index,
                     bool showUnsigned,
                     std::vector<Candidate>& outTop,
                     std::vector<Candidate>& outAlternatives);

    // Check if a catalog signature is valid (cached in index record eventually).
    // For now performs live check.
    static bool IsSignatureValid(const std::filesystem::path& catalogPath);

    // Signature state for an indexed record: we can only judge a package whose
    // catalog is actually present on disk.
    enum class Signature { Unknown, Valid, Invalid };

    // Score breakdown for unit testing.
    struct ScoreBreakdown {
        int exactPrimary = 0;       // most specific HW ID match
        int exactOther = 0;         // other HW ID matches
        int compatibleOnly = 0;     // compatible ID match only
        int classGuidBonus = 0;
        int newerDateBonus = 0;
        int newerVersionBonus = 0;
        int signatureBonus = 0;
        int unsignedPenalty = 0;
        int manufacturerBonus = 0;
        int problemDeviceBonus = 0;
        int total = 0;
    };
    static ScoreBreakdown ComputeScore(const Hw::Device& device,
                                       const IndexRecord& record,
                                       const PackIndex& index,
                                       Signature signature);
};

} // namespace DriverNest::Packs
