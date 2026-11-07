#include "Ranker.h"
#include "InfParser.h"
#include "Log.h"
#include "Verify.h"
#include "Strings.h"
#include <windows.h>
#include <algorithm>
#include <cmath>

namespace DriverNest::Packs {

// Defined at the bottom of this file.
std::vector<std::wstring> BuildReasonTags(const Ranker::ScoreBreakdown& bd);

namespace {

// Compare version strings for "greater than" semantics.
bool IsVersionGreater(std::wstring_view a, std::wstring_view b) {
    return Hw::CompareVersion(a, b) > 0;
}

// Compare FILETIME epochs.
bool IsDateNewer(std::int64_t a, std::int64_t b) {
    return a > b;
}

// Lowercase UTF-8 conversion for case-insensitive hardware ID comparison.
std::string ToLowerUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                      nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        out.data(), n, nullptr, nullptr);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// Convert UTF-8 to wide for display in the candidate record.
std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

// ASCII lowercase for already-UTF-8 strings (hardware IDs are ASCII in practice).
std::string ToLowerUtf8(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

} // namespace

void Ranker::Rank(const Hw::Device& device,
                 const PackIndex& index,
                 bool showUnsigned,
                 std::vector<Candidate>& outTop,
                 std::vector<Candidate>& outAlternatives) {
    outTop.clear();
    outAlternatives.clear();

    if (device.hardwareIds.empty() && device.compatibleIds.empty()) {
        return;
    }

    // Collect all candidate records that match any of the device's hardware or
    // compatible IDs.
    struct Scored {
        Candidate cand;
        ScoreBreakdown breakdown;
    };
    std::vector<Scored> scored;

    // For each record in the index, check if it matches.
    // This is O(records); in production we'd use the hash index, but for now
    // we do a full scan. The hash index is used for lookup in the full impl.

    for (size_t i = 0; i < index.records.size(); ++i) {
        const auto& rec = index.records[i];

        // Reject non-matching packages before touching the filesystem or
        // verifying catalog signatures. Signature verification is expensive.
        ScoreBreakdown bd = ComputeScore(device, rec, index, Signature::Unknown);
        if (bd.exactPrimary == 0 && bd.exactOther == 0 && bd.compatibleOnly == 0) {
            continue;
        }

        // Establish the signature state. A package whose INF is not on disk
        // cannot be judged, so it stays Unknown and is not penalised.
        Signature signature = Signature::Unknown;
        bool catalogPresent = false;
        if (!index.rootPath.empty() && !rec.infRelPath.empty()) {
            const std::filesystem::path infPath =
                index.rootPath / std::filesystem::u8path(rec.infRelPath);
            if (std::filesystem::exists(infPath)) {
                std::filesystem::path catPath = infPath;
                catPath.replace_extension(L".cat");
                catalogPresent = std::filesystem::exists(catPath);
                signature = catalogPresent && IsSignatureValid(catPath)
                                ? Signature::Valid
                                : Signature::Invalid;
            }
        }

        // Unsigned packages are hidden unless the user opted in.
        if (signature == Signature::Invalid && !showUnsigned) {
            continue;
        }

        bd = ComputeScore(device, rec, index, signature);

        Candidate cand;
        cand.recordIndex = i;
        cand.score = bd.total;
        cand.hardwareIdMatched = Utf8ToWide(rec.hardwareId);
        cand.signedOK = signature == Signature::Valid;
        cand.catalogPresent = catalogPresent;
        cand.reasonTags = BuildReasonTags(bd);

        Scored s;
        s.cand = std::move(cand);
        s.breakdown = std::move(bd);
        scored.push_back(std::move(s));
    }

    // Sort by score descending, then by date descending, then by version descending,
    // then by path length ascending.
    std::sort(scored.begin(), scored.end(),
              [&index](const Scored& a, const Scored& b) {
                  if (a.cand.score != b.cand.score) {
                      return a.cand.score > b.cand.score;
                  }
                  // date newer first
                  if (index.records[a.cand.recordIndex].dateEpoch !=
                      index.records[b.cand.recordIndex].dateEpoch) {
                      return index.records[a.cand.recordIndex].dateEpoch >
                             index.records[b.cand.recordIndex].dateEpoch;
                  }
                  // version newer first
                  if (index.records[a.cand.recordIndex].version !=
                      index.records[b.cand.recordIndex].version) {
                      return index.records[a.cand.recordIndex].version >
                             index.records[b.cand.recordIndex].version;
                  }
                  // shorter path first
                  return index.records[a.cand.recordIndex].infRelPath.size() <
                         index.records[b.cand.recordIndex].infRelPath.size();
              });

    // Take top candidate
    if (!scored.empty()) {
        outTop.push_back(std::move(scored[0].cand));
        // Next two alternatives (if score > 0 and different hardware ID)
        for (size_t i = 1; i < std::min(scored.size(), size_t(3)); ++i) {
            if (scored[i].cand.score > 0) {
                outAlternatives.push_back(std::move(scored[i].cand));
            }
        }
    }
}

bool Ranker::IsSignatureValid(const std::filesystem::path& catalogPath) {
    // Delegate to Verify module
    return DriverNest::Install::VerifyCatalogSignature(catalogPath);
}

Ranker::ScoreBreakdown Ranker::ComputeScore(const Hw::Device& device,
                                             const IndexRecord& record,
                                             const PackIndex& /*index*/,
                                             Signature signature) {
    ScoreBreakdown bd;
    int score = 0;

    // Get the record's INF path to look up its data
    // For now we don't have the InfEntry directly; we'd need to store it in
    // the index or re-parse. Simplified: we use record fields.

    // Check hardware ID matches
    for (size_t pos = 0; pos < device.hardwareIds.size(); ++pos) {
        std::string devIdLower = ToLowerUtf8(device.hardwareIds[pos]);
        std::string recIdLower = ToLowerUtf8(record.hardwareId);

        if (devIdLower == recIdLower) {
            if (pos == 0) {
                // Exact match of most specific hardware ID
                bd.exactPrimary = 1000;
                score += 1000;
            } else {
                int bonus = 800 - static_cast<int>(pos) * 20;
                bd.exactOther += bonus;
                score += bonus;
            }
            break; // Only count first match per device ID
        }
    }

    // Check compatible ID match (only if no hardware ID matched)
    if (bd.exactPrimary == 0 && bd.exactOther == 0) {
        for (auto& cid : device.compatibleIds) {
            std::string cidLower = ToLowerUtf8(cid);
            std::string recIdLower = ToLowerUtf8(record.hardwareId);
            if (cidLower == recIdLower) {
                bd.compatibleOnly = 400;
                score += 400;
                break;
            }
        }
    }

    // Class GUID match
    if (!device.classGuid.empty() && !record.hardwareId.empty()) {
        // We don't have the class GUID in the record directly; it should be in
        // the INF. For now skip.
        // bd.classGuidBonus would be set if we had the data.
    }

    // Newer date bonus
    if (device.installedDate.dwLowDateTime != 0 || device.installedDate.dwHighDateTime != 0) {
        std::int64_t installedEpoch = Hw::FileTimeToEpoch(device.installedDate);
        if (IsDateNewer(record.dateEpoch, installedEpoch)) {
            bd.newerDateBonus = 30;
            score += 30;
        }
    }

    // Newer version bonus
    if (!device.installedVersion.empty()) {
        std::wstring recVersion = PackIndex::UnpackVersion(record.version);
        if (IsVersionGreater(recVersion, device.installedVersion)) {
            bd.newerVersionBonus = 40;
            score += 40;
        }
    }

    // Signature bonus / penalty. Unknown state leaves the score untouched so a
    // package with no catalog on disk is not treated as malicious.
    if (signature == Signature::Valid) {
        bd.signatureBonus = 100;
        score += 100;
    } else if (signature == Signature::Invalid) {
        bd.unsignedPenalty = -500;
        score -= 500;
    }

    // Manufacturer bonus
    if (!device.manufacturer.empty() && !record.hardwareId.empty()) {
        // Would need provider info from INF; skip for now.
    }

    // Problem device bonus
    if (device.problemCode != 0) {
        bd.problemDeviceBonus = 25;
        score += 25;
    }

    bd.total = score;
    return bd;
}

std::vector<std::wstring> BuildReasonTags(const Ranker::ScoreBreakdown& bd) {
    std::vector<std::wstring> tags;

    auto add = [&tags](std::wstring_view v) { tags.emplace_back(v); };

    if (bd.exactPrimary > 0 || bd.exactOther > 0) {
        add(Strings::ReasonExactId);
    }

    if (bd.compatibleOnly > 0) {
        add(Strings::ReasonCompatibleId);
    }

    if (bd.newerVersionBonus > 0) {
        add(Strings::ReasonNewerVersion);
    }

    if (bd.newerDateBonus > 0) {
        add(Strings::ReasonNewerDate);
    }

    if (bd.signatureBonus > 0) {
        add(Strings::ReasonSigned);
    }

    if (bd.classGuidBonus > 0) {
        add(Strings::ReasonClassMatch);
    }

    if (bd.manufacturerBonus > 0) {
        add(Strings::ReasonManufacturer);
    }

    if (bd.problemDeviceBonus > 0) {
        add(Strings::ReasonProblemDevice);
    }

    if (bd.unsignedPenalty < 0) {
        add(Strings::ReasonUnsigned);
    }

    return tags;
}

} // namespace DriverNest::Packs
