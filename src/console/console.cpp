#include "console/console.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "util/log.hpp"

namespace manifold {

namespace {

/// Parses all of `text` as a number with std::from_chars.
///
/// \throws CommandError with `what` in the message if that fails.
template <typename Number>
Number parse_number(const std::string& text, std::string_view what) {
    Number value{};
    const char* end = text.data() + text.size();
    // std::from_chars doesn't take a plus sign, but people type it.
    const char* begin = text.starts_with('+') && !text.starts_with("+-") ? text.data() + 1 : text.data();
    const auto [stop, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || stop != end) {
        throw CommandError(std::format("not {}: {}", what, text));
    }
    return value;
}

}  // namespace

std::vector<std::vector<std::string>> parse_commands(std::string_view text) {
    std::vector<std::vector<std::string>> commands(1);
    std::string word;
    const auto end_word = [&] {
        if (!word.empty()) {
            commands.back().push_back(std::move(word));
            word.clear();
        }
    };
    for (const char c : text) {
        if (c == ';' || c == '\n' || c == '\r') {
            end_word();
            if (!commands.back().empty()) {
                commands.emplace_back();
            }
        } else if (c == ' ' || c == '\t') {
            end_word();
        } else {
            word += c;
        }
    }
    end_word();
    if (commands.back().empty()) {
        commands.pop_back();
    }
    return commands;
}

float parse_float_argument(const std::string& text) {
    const auto value = parse_number<float>(text, "a number");
    if (!std::isfinite(value)) {
        throw CommandError("not a finite number: " + text);
    }
    return value;
}

int parse_int_argument(const std::string& text) { return parse_number<int>(text, "an integer"); }

bool parse_switch_argument(const std::string& text) {
    if (text == "on") {
        return true;
    }
    if (text == "off") {
        return false;
    }
    throw CommandError("expected on or off, not " + text);
}

std::vector<std::string> LineBuffer::take_lines() {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t end = pending_.find('\n'); end != std::string::npos; end = pending_.find('\n', start)) {
        lines.push_back(pending_.substr(start, end - start));
        start = end + 1;
    }
    pending_.erase(0, start);
    return lines;
}

Console::Console() {
    add("help", "help", "Lists all commands.", [this](std::span<const std::string> arguments) {
        if (!arguments.empty()) {
            throw CommandError("usage: help");
        }
        log_info("Commands:\n{}", help());
    });
}

void Console::add(const std::string& name, std::string usage, std::string help, Handler handler) {
    if (!commands_.try_emplace(name, Command{std::move(usage), std::move(help), std::move(handler)}).second) {
        throw std::invalid_argument("console command added twice: " + name);
    }
}

void Console::run(std::span<const std::string> words) const {
    if (words.empty()) {
        return;
    }
    const auto found = commands_.find(words.front());
    if (found == commands_.end()) {
        throw CommandError("unknown command: " + words.front() + " (type help for a list)");
    }
    found->second.handler(words.subspan(1));
}

std::string Console::help() const {
    std::string text;
    for (const auto& [name, command] : commands_) {
        text += std::format("  {:36} {}\n", command.usage, command.help);
    }
    return text;
}

void CommandQueue::add(std::string_view text, bool stop_on_error) {
    for (std::vector<std::string>& words : parse_commands(text)) {
        commands_.push_back({.words = std::move(words), .stop_on_error = stop_on_error});
    }
}

std::size_t CommandQueue::clear() {
    const std::size_t count = commands_.size();
    commands_.clear();
    frames_to_wait_ = 0;
    return count;
}

void CommandQueue::run_frame(const Console& console) {
    if (frames_to_wait_ > 0) {
        --frames_to_wait_;
        if (frames_to_wait_ > 0) {
            return;
        }
    }
    while (!commands_.empty() && frames_to_wait_ == 0) {
        const Entry entry = std::move(commands_.front());
        commands_.pop_front();
        running_stops_on_error_ = entry.stop_on_error;
        try {
            console.run(entry.words);
        } catch (const CommandError& error) {
            std::string command;
            for (const std::string& word : entry.words) {
                command += (command.empty() ? "" : " ") + word;
            }
            if (entry.stop_on_error) {
                throw CommandError(std::format("{}: {}", command, error.what()));
            }
            log_error("{}: {}", command, error.what());
        }
    }
}

}  // namespace manifold
