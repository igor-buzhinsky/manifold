#include "settings/settings.hpp"

#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "util/log.hpp"
#include "util/text_file.hpp"

namespace manifold {

namespace {

/// The value of the boolean field `name`.
///
/// \throws std::runtime_error if `value` isn't a boolean.
bool read_bool(const std::string& name, const nlohmann::json& value) {
    if (!value.is_boolean()) {
        throw std::runtime_error("invalid " + name + ": " + value.dump() + " (expected true or false)");
    }
    return value.get<bool>();
}

/// The value of the field `name`, a number greater than 0 that fits a float.
///
/// \throws std::runtime_error if `value` is something else. A number too large or too small for a float is rejected
///         too, rather than becoming infinite or 0.
float read_positive_float(const std::string& name, const nlohmann::json& value) {
    const double number = value.is_number() ? value.get<double>() : 0.0;
    if (number < std::numeric_limits<float>::min() || number > std::numeric_limits<float>::max()) {
        throw std::runtime_error("invalid " + name + ": " + value.dump() +
                                 " (expected a number > 0 that fits a float)");
    }
    return static_cast<float>(number);
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
    // Unknown fields are logged only once the whole file is valid, since an invalid file is skipped.
    std::vector<std::string> unknown;
    for (const auto& [name, value] : object.items()) {
        if (name == "fullscreen") {
            settings.fullscreen = read_bool(name, value);
        } else if (name == "mouse_look") {
            settings.mouse_look = read_bool(name, value);
        } else if (name == "mouse_sensitivity") {
            settings.mouse_sensitivity = read_positive_float(name, value);
        } else if (name == "autosave") {
            settings.autosave = read_bool(name, value);
        } else if (name == "autosave_interval") {
            settings.autosave_interval = read_positive_float(name, value);
        } else {
            unknown.push_back(name);
        }
    }
    for (const std::string& name : unknown) {
        log_warning("Unknown setting ignored: {}", name);
    }
    return settings;
}

Settings load_settings(std::span<const std::filesystem::path> files) {
    Settings settings;
    for (const std::filesystem::path& path : files) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            if (error) {
                log_warning("Settings: {} skipped: {}", path.string(), error.message());
            }
            continue;
        }
        try {
            settings = parse_settings(read_text_file(path), settings);
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
