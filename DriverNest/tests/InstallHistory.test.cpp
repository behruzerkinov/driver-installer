#include "test_framework.h"
#include "install/InstallHistory.h"

using namespace DriverNest::Install;

TEST(InstallHistoryRecordAndMatchInstanceId) {
    InstallHistory::Clear();

    SessionRecord rec{};
    rec.instanceId = L"PCI\\VEN_8086&DEV_15F3&SUBSYS_00008086&REV_02\\3&11583659&0&C8";
    rec.description = L"Intel(R) Ethernet Controller I225-V";
    rec.hardwareIds = { L"PCI\\VEN_8086&DEV_15F3&REV_02", L"PCI\\VEN_8086&DEV_15F3" };
    rec.publishedInf = L"oem42.inf";
    rec.targetVersion = L"1.0.2.14";
    rec.restartRequired = true;

    InstallHistory::RecordInstall(rec);

    TEST_ASSERT(InstallHistory::IsWaitingRestart(
        L"PCI\\VEN_8086&DEV_15F3&SUBSYS_00008086&REV_02\\3&11583659&0&C8", {}));

    // Case insensitivity check
    TEST_ASSERT(InstallHistory::IsWaitingRestart(
        L"pci\\ven_8086&dev_15f3&subsys_00008086&rev_02\\3&11583659&0&c8", {}));

    TEST_ASSERT_EQ(InstallHistory::GetPendingRestartCount(), 1);

    InstallHistory::Clear();
}

TEST(InstallHistoryMatchByHardwareId) {
    InstallHistory::Clear();

    SessionRecord rec{};
    rec.instanceId = L"USB\\VID_0BDA&PID_8153\\000001";
    rec.description = L"Realtek USB GbE Family Controller";
    rec.hardwareIds = { L"USB\\VID_0BDA&PID_8153&REV_3000", L"USB\\VID_0BDA&PID_8153" };
    rec.publishedInf = L"oem55.inf";
    rec.restartRequired = true;

    InstallHistory::RecordInstall(rec);

    // Query with different instanceId but matching hardwareId
    std::vector<std::wstring> queryHwIds = { L"USB\\VID_0BDA&PID_8153" };
    TEST_ASSERT(InstallHistory::IsWaitingRestart(L"SOME_OTHER_INSTANCE_ID", queryHwIds));

    // Query with non-matching hardwareId
    std::vector<std::wstring> nonMatching = { L"PCI\\VEN_10DE&DEV_1F08" };
    TEST_ASSERT(!InstallHistory::IsWaitingRestart(L"OTHER_DEVICE", nonMatching));

    InstallHistory::Clear();
}

TEST(InstallHistoryPendingCountAndClear) {
    InstallHistory::Clear();
    TEST_ASSERT_EQ(InstallHistory::GetPendingRestartCount(), 0);

    SessionRecord r1{};
    r1.instanceId = L"DEV_1";
    r1.restartRequired = true;
    InstallHistory::RecordInstall(r1);

    SessionRecord r2{};
    r2.instanceId = L"DEV_2";
    r2.restartRequired = false;
    InstallHistory::RecordInstall(r2);

    TEST_ASSERT_EQ(InstallHistory::GetPendingRestartCount(), 1);

    InstallHistory::Clear();
    TEST_ASSERT_EQ(InstallHistory::GetPendingRestartCount(), 0);
}
