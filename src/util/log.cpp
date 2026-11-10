#include "util/log.hpp"

#include <iostream>
#include <string_view>

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

}  // namespace

void log_message(LogLevel level, std::string_view message) { std::cerr << level_prefix(level) << message << '\n'; }

}  // namespace manifold
