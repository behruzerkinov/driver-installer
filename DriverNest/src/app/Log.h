#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <cstdint>

namespace DriverNest::App {

// ---------------------------------------------------------------------------
// Log levels
// ---------------------------------------------------------------------------
enum class LogLevel : std::int32_t {
    Error = 0,
    Warn,
    Info,
    Debug,
};

// ---------------------------------------------------------------------------
// Configure the rotating log. Called once at startup.
//   pathBase  - directory + base filename without extension, e.g.
//               C:\Users\...\AppData\Local\DriverNest\driver_nest.log
//   level     - minimum severity to write
//   debugFlag - true when --debug was passed
// ---------------------------------------------------------------------------
void LogInitialize(std::filesystem::path pathBase, LogLevel level, bool debugFlag);

// ---------------------------------------------------------------------------
// Write a log entry. Thread-safe.
// ---------------------------------------------------------------------------
void LogMessage(LogLevel level, std::wstring_view message);
void LogMessageF(LogLevel level, std::wstring_view fmt, ...);

// Convenience wrappers
[[noreturn]] void LOG_FATAL(std::wstring_view message);
void LOG_ERROR(std::wstring_view message);
void LOG_WARN(std::wstring_view message);
void LOG_INFO(std::wstring_view message);
void LOG_DEBUG(std::wstring_view message);

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
[[nodiscard]] LogLevel CurrentLogLevel();
[[nodiscard]] bool DebugEnabled();

} // namespace DriverNest::App
