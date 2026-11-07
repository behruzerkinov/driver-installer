#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <windows.h>

namespace DriverNest::Install {

// Record of a driver installed in the current session.
struct SessionRecord {
    std::wstring instanceId;
    std::wstring description;
    std::vector<std::wstring> hardwareIds;
    std::wstring publishedInf;
    std::wstring targetVersion;
    bool restartRequired = true;
    std::chrono::system_clock::time_point installTime;
};

class InstallHistory {
public:
    // Record a completed installation
    static void RecordInstall(const SessionRecord& rec);

    // Check if a device (by instance ID or hardware IDs) was installed in this session and is waiting restart
    static bool IsWaitingRestart(const std::wstring& instanceId,
                                 const std::vector<std::wstring>& hardwareIds);

    // Get the session record matching a device
    static const SessionRecord* FindRecord(const std::wstring& instanceId,
                                          const std::vector<std::wstring>& hardwareIds);

    // All records in this session
    static const std::vector<SessionRecord>& GetSessionRecords();

    // Count of drivers waiting restart
    static int GetPendingRestartCount();

    // Clear session records (e.g. on new boot or explicit clear)
    static void Clear();
};

} // namespace DriverNest::Install
