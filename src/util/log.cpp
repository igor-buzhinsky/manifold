#include "util/log.hpp"

#include <iostream>
#include <string_view>
#include <utility>

namespace manifold {

namespace {

/// The prefix that shows the level of a message.
std::string_view level_prefix(LogLevel level) {
    switch (level) {
        case LogLevel::kInfo:
            return "[info] ";
        case LogLevel::kWarning:
            return "[warning] ";
        case LogLevel::kError:
            return "[error] ";
    }
    return "";
}

/// The listener of set_log_listener().
LogListener& current_listener() {
    static LogListener instance;
    return instance;
}

}  // namespace

void log_message(LogLevel level, std::string_view message) {
    std::cerr << level_prefix(level) << message << '\n';
    if (current_listener()) {
        current_listener()(level, message);
    }
}

void set_log_listener(LogListener listener) { current_listener() = std::move(listener); }

}  // namespace manifold
