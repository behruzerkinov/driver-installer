#include "App.h"
#include <windows.h>
#include <dbghelp.h>
#include <exception>
#include <memory>
#include <cstdio>

// ---------------------------------------------------------------------------
// Global unhandled exception filter: write a minidump and show a dialog.
// ---------------------------------------------------------------------------
LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* ep) {
    // Write the minidump into the app data "crash" folder.
    auto dir = DriverNest::App::AppDataDirectory() / "crash";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::wstring dumpPath = (dir / (L"crash_" + std::to_wstring(GetCurrentProcessId()) +
                                    L".dmp")).wstring();

    HANDLE hFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exInfo{};
        exInfo.ThreadId = GetCurrentThreadId();
        exInfo.ExceptionPointers = ep;
        exInfo.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory |
                                                     MiniDumpWithHandleData |
                                                     MiniDumpWithUnloadedModules),
                          &exInfo, nullptr, nullptr);
        CloseHandle(hFile);
    }

    const std::wstring message =
        L"DriverNest encountered an unexpected error and has closed.\n\n"
        L"A crash dump has been saved to:\n" + dumpPath +
        L"\n\nPlease report this to the developers.";

    MessageBoxW(nullptr, message.c_str(),
                L"DriverNest - Unexpected Error",
                MB_ICONERROR | MB_OK | MB_SETFOREGROUND);

    ExitProcess(1);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int wmain(int argc, wchar_t* argv[]) {
    // Set up unhandled exception filter
    SetUnhandledExceptionFilter(HandleUnhandledException);

    DriverNest::App::App app;
    if (!app.Initialize(argc, argv)) {
        // Initialize returned false: elevation missing or another instance.
        return 1;
    }

    return app.Run();
}
