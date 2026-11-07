#include "InstallHistory.h"
#include "app/Log.h"
#include <algorithm>
#include <mutex>
#include <cwctype>

namespace DriverNest::Install {

namespace {

std::vector<SessionRecord> g_sessionRecords;
std::mutex g_historyMutex;

std::wstring ToLower(std::wstring_view s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t ch : s) {
        out.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return out;
}

} // namespace

void InstallHistory::RecordInstall(const SessionRecord& rec) {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    // If instanceId already recorded, update it
    auto it = std::find_if(g_sessionRecords.begin(), g_sessionRecords.end(),
        [&](const SessionRecord& existing) {
            return !rec.instanceId.empty() &&
                   ToLower(existing.instanceId) == ToLower(rec.instanceId);
        });

    if (it != g_sessionRecords.end()) {
        *it = rec;
    } else {
        g_sessionRecords.push_back(rec);
    }

    App::LOG_INFO(L"InstallHistory: recorded driver installation for " +
                  rec.description + L" (Reboot pending=" +
                  (rec.restartRequired ? L"true" : L"false") + L")");
}

bool InstallHistory::IsWaitingRestart(const std::wstring& instanceId,
                                     const std::vector<std::wstring>& hardwareIds) {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    const std::wstring lowerInst = ToLower(instanceId);

    for (const auto& rec : g_sessionRecords) {
        if (!rec.restartRequired) continue;

        if (!lowerInst.empty() && ToLower(rec.instanceId) == lowerInst) {
            return true;
        }

        // Check if any hardware IDs match
        for (const auto& hid : hardwareIds) {
            const std::wstring lowerHid = ToLower(hid);
            for (const auto& recHid : rec.hardwareIds) {
                if (ToLower(recHid) == lowerHid) {
                    return true;
                }
            }
        }
    }
    return false;
}

const SessionRecord* InstallHistory::FindRecord(const std::wstring& instanceId,
                                              const std::vector<std::wstring>& hardwareIds) {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    const std::wstring lowerInst = ToLower(instanceId);

    for (const auto& rec : g_sessionRecords) {
        if (!lowerInst.empty() && ToLower(rec.instanceId) == lowerInst) {
            return &rec;
        }
        for (const auto& hid : hardwareIds) {
            const std::wstring lowerHid = ToLower(hid);
            for (const auto& recHid : rec.hardwareIds) {
                if (ToLower(recHid) == lowerHid) {
                    return &rec;
                }
            }
        }
    }
    return nullptr;
}

const std::vector<SessionRecord>& InstallHistory::GetSessionRecords() {
    return g_sessionRecords;
}

int InstallHistory::GetPendingRestartCount() {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    int count = 0;
    for (const auto& rec : g_sessionRecords) {
        if (rec.restartRequired) ++count;
    }
    return count;
}

void InstallHistory::Clear() {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    g_sessionRecords.clear();
}

} // namespace DriverNest::Install
