#include "Log.h"
#include <windows.h>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <atomic>
#include <cwchar>

namespace DriverNest::App {

namespace {

// Maximum log file size (1 MB) and number of rolling files to keep.
constexpr std::size_t MaxLogSize = 1 * 1024 * 1024;
constexpr int MaxRollFiles = 3;

// Thread-safe logger state.
struct Logger {
    std::mutex mtx;
    std::ofstream file;
    std::filesystem::path pathBase;
    LogLevel level = LogLevel::Info;
    bool debug = false;

    void ensureFile() {
        if (!file.is_open()) {
            file.open(pathBase.wstring(), std::ios::out | std::ios::app | std::ios::binary);
        }
        // Rotate if needed
        if (file.is_open()) {
            file.seekp(0, std::ios::end);
            auto pos = file.tellp();
            if (pos > static_cast<std::streamoff>(MaxLogSize)) {
                file.close();
                RotateFiles();
                file.open(pathBase.wstring(), std::ios::out | std::ios::app | std::ios::binary);
            }
        }
    }

    void RotateFiles() {
        // *.log.2 -> *.log.3, *.log.1 -> *.log.2, *.log -> *.log.1
        for (int i = MaxRollFiles; i >= 1; --i) {
            auto src = (i == 1) ? pathBase : pathBase.parent_path() / (pathBase.filename().string() + "." + std::to_string(i));
            auto dst = (i == MaxRollFiles) ? pathBase.parent_path() / (pathBase.filename().string() + "." + std::to_string(i) + ".old") : pathBase.parent_path() / (pathBase.filename().string() + "." + std::to_string(i + 1));
            if (i == 1) {
                // For the main log, move to .1
                std::filesystem::path dstPath = pathBase.parent_path() / (pathBase.filename().string() + ".1");
                std::error_code ec;
                std::filesystem::rename(pathBase, dstPath, ec);
            } else {
                auto srcPath = pathBase.parent_path() / (pathBase.filename().string() + "." + std::to_string(i));
                auto dstPath = pathBase.parent_path() / (pathBase.filename().string() + "." + std::to_string(i + 1));
                std::error_code ec;
                if (std::filesystem::exists(srcPath, ec) && !ec) {
                    std::filesystem::rename(srcPath, dstPath, ec);
                }
            }
        }
    }
};

Logger& logger() {
    static Logger l;
    return l;
}

void WriteEntry(LogLevel level, std::wstring_view message) {
    if (level < logger().level) return;

    std::lock_guard<std::mutex> lock(logger().mtx);
    logger().ensureFile();

    auto& f = logger().file;
    if (!f.is_open()) return;

    // Prefix with timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf(timeBuf, 64, L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] ",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    f.write(reinterpret_cast<const char*>(timeBuf),
            static_cast<std::streamsize>(wcslen(timeBuf) * sizeof(wchar_t)));

    // Level tag
    wchar_t levelTag[8];
    switch (level) {
        case LogLevel::Error: wcscpy_s(levelTag, 8, L"ERROR"); break;
        case LogLevel::Warn:  wcscpy_s(levelTag, 8, L"WARN "); break;
        case LogLevel::Info:  wcscpy_s(levelTag, 8, L"INFO "); break;
        case LogLevel::Debug: wcscpy_s(levelTag, 8, L"DEBUG"); break;
    }
    f.write(reinterpret_cast<const char*>(levelTag),
            static_cast<std::streamsize>(wcslen(levelTag) * sizeof(wchar_t)));
    static constexpr wchar_t kSep[] = L" | ";
    f.write(reinterpret_cast<const char*>(kSep), static_cast<std::streamsize>(3 * sizeof(wchar_t)));

    f.write(reinterpret_cast<const char*>(message.data()),
            static_cast<std::streamsize>(message.size() * sizeof(wchar_t)));
    static constexpr wchar_t kNewline[] = L"\n";
    f.write(reinterpret_cast<const char*>(kNewline), static_cast<std::streamsize>(sizeof(wchar_t)));

    f.flush();
}

} // namespace

void LogInitialize(std::filesystem::path pathBase, LogLevel level, bool debugFlag) {
    logger().pathBase = std::move(pathBase);
    logger().level = level;
    logger().debug = debugFlag;
    // Ensure directory exists
    std::error_code ec;
    std::filesystem::create_directories(logger().pathBase.parent_path(), ec);
}

void LogMessage(LogLevel level, std::wstring_view message) {
    WriteEntry(level, message);
}

void LogMessageF(LogLevel level, std::wstring_view fmt, ...) {
    // Simple variadic fallback; not essential for v1. To keep size down we
    // implement a minimal formatter for common patterns. For production we'd
    // use a proper library, but here a direct WriteEntry with pre-formatted
    // wide string is sufficient and avoids bringing in extra deps.
    va_list args;
    va_start(args, fmt);
    wchar_t buf[2048];
    int len = vswprintf(buf, 2048, fmt.data(), args);
    va_end(args);
    if (len > 0) {
        WriteEntry(level, std::wstring_view(buf, static_cast<std::size_t>(len)));
    }
}

[[noreturn]] void LOG_FATAL(std::wstring_view message) {
    WriteEntry(LogLevel::Error, message);
    // In a real app we'd also write a minidump here; done in main.cpp via
    // SetUnhandledExceptionFilter. For a fatal log we exit immediately.
    std::exit(EXIT_FAILURE);
}

void LOG_ERROR(std::wstring_view message) { WriteEntry(LogLevel::Error, message); }
void LOG_WARN(std::wstring_view message)  { WriteEntry(LogLevel::Warn, message); }
void LOG_INFO(std::wstring_view message)  { WriteEntry(LogLevel::Info, message); }
void LOG_DEBUG(std::wstring_view message) { WriteEntry(LogLevel::Debug, message); }

LogLevel CurrentLogLevel() { return logger().level; }
bool DebugEnabled() { return logger().debug; }

} // namespace DriverNest::App
