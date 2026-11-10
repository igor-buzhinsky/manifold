#pragma once

#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

namespace manifold {

/// How important a log message is.
enum class LogLevel : std::uint8_t {
    kInfo,     ///< Normal progress, e.g. what level was generated.
    kWarning,  ///< Something unusual that still works, e.g. a slow level generation.
    kError,    ///< Something failed.
};

/// Writes a message to stderr, prefixed with its level, e.g. "[warning] ...".
void log_message(LogLevel level, std::string_view message);

/// Writes an info message, formatted like std::format().
template <typename... Args>
void log_info(std::format_string<Args...> format, Args&&... args) {
    log_message(LogLevel::kInfo, std::format(format, std::forward<Args>(args)...));
}

/// Writes a warning message, formatted like std::format().
template <typename... Args>
void log_warning(std::format_string<Args...> format, Args&&... args) {
    log_message(LogLevel::kWarning, std::format(format, std::forward<Args>(args)...));
}

/// Writes an error message, formatted like std::format().
template <typename... Args>
void log_error(std::format_string<Args...> format, Args&&... args) {
    log_message(LogLevel::kError, std::format(format, std::forward<Args>(args)...));
}

}  // namespace manifold
