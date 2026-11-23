#include "settings/settings.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "util/log.hpp"

namespace manifold {

namespace {

/// Reads a whole file.
///
/// \throws std::runtime_error if `path` is not a regular file or can't be opened or read.
std::string read_file(const std::filesystem::path& path) {
    // A directory can be opened as a stream on some platforms, and then reads as empty.
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        throw std::runtime_error("not a file");
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("can't open " + path.string());
    }
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        throw std::runtime_error("can't read " + path.string());
    }
    return text;
}

}  // namespace

Settings parse_settings(std::string_view json, const Settings& base) {
    nlohmann::json object;
    try {
        object = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error& error) {
        throw std::runtime_error(std::string("invalid JSON: ") + error.what());
    }
    if (!object.is_object()) {
        throw std::runtime_error("the settings must be a JSON object");
    }
    Settings settings = base;
    for (const auto& [name, value] : object.items()) {
        if (name == "fullscreen") {
            if (!value.is_boolean()) {
                throw std::runtime_error("invalid fullscreen: " + value.dump() + " (expected true or false)");
            }
            settings.fullscreen = value.get<bool>();
        } else {
            log_warning("Unknown setting ignored: {}", name);
        }
    }
    return settings;
}

Settings load_settings(std::span<const std::filesystem::path> files) {
    Settings settings;
    for (const std::filesystem::path& path : files) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            continue;
        }
        try {
            settings = parse_settings(read_file(path), settings);
            log_info("Settings: read {}", path.string());
        } catch (const std::runtime_error& failure) {
            log_error("Settings: {} skipped: {}", path.string(), failure.what());
        }
    }
    return settings;
}

std::filesystem::path find_game_settings_file(const std::filesystem::path& executable_dir,
                                              const std::filesystem::path& fallback) {
    if (!executable_dir.empty()) {
        std::filesystem::path next_to_executable = executable_dir / kSettingsFileName;
        std::error_code error;
        if (std::filesystem::is_regular_file(next_to_executable, error)) {
            return next_to_executable;
        }
    }
    return fallback;
}

}  // namespace manifold
