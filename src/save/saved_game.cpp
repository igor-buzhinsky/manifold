#include "save/saved_game.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "level/layout_factory.hpp"

namespace manifold {

namespace {

/// Indentation of the JSON, in spaces, so the file can be read and edited by hand.
constexpr int kJsonIndent = 2;

/// The field `name` of a save.
///
/// \throws std::runtime_error if there is no such field.
const nlohmann::json& field(const nlohmann::json& object, const char* name) {
    const auto found = object.find(name);
    if (found == object.end()) {
        throw std::runtime_error(std::string("save has no ") + name);
    }
    return *found;
}

/// Throws std::runtime_error for a field of a save that has a wrong value.
[[noreturn]] void throw_invalid(const char* name, const nlohmann::json& value, const char* expected) {
    throw std::runtime_error(std::string("save has an invalid ") + name + ", " + value.dump() + ": expected " +
                             expected);
}

}  // namespace

std::string save_to_json(const SavedGame& save) {
    nlohmann::ordered_json json = {
        {"version", kSaveFormatVersion},
        {"seed", save.seed},
        {"level_number", save.level_number},
        {"time", save.time},
    };
    if (save.layout) {
        json["layout"] = std::string(layout_name(*save.layout));
    }
    return json.dump(kJsonIndent) + "\n";
}

SavedGame parse_saved_game(std::string_view json) {
    nlohmann::json object;
    try {
        object = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error& error) {
        throw std::runtime_error(std::string("save is not valid JSON: ") + error.what());
    }
    if (!object.is_object()) {
        throw std::runtime_error("save is not a JSON object");
    }
    // A number with a fraction would be read as an integer without it, so the types are checked first.
    const nlohmann::json& version = field(object, "version");
    if (!version.is_number_integer() || version.get<std::int64_t>() != kSaveFormatVersion) {
        throw std::runtime_error("save has version " + version.dump() + ", but only version " +
                                 std::to_string(kSaveFormatVersion) + " can be read");
    }

    SavedGame save;
    const nlohmann::json& seed = field(object, "seed");
    if (!seed.is_number_unsigned()) {
        throw_invalid("seed", seed, "a non-negative integer");
    }
    save.seed = seed.get<std::uint64_t>();

    const nlohmann::json& level_number = field(object, "level_number");
    if (!level_number.is_number_unsigned() ||
        level_number.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw_invalid("level_number", level_number, "a non-negative integer");
    }
    save.level_number = level_number.get<int>();

    const nlohmann::json& time = field(object, "time");
    if (!time.is_number() || !std::isfinite(time.get<double>()) || time.get<double>() < 0.0) {
        throw_invalid("time", time, "a non-negative number");
    }
    save.time = time.get<double>();

    if (const auto layout = object.find("layout"); layout != object.end()) {
        if (layout->is_string()) {
            save.layout = parse_layout_name(layout->get<std::string>());
        }
        if (!save.layout) {
            throw_invalid("layout", *layout, "debug or rooms");
        }
    }
    return save;
}

void write_save_file(const std::filesystem::path& path, const SavedGame& save) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << save_to_json(save);
        file.close();
        if (!file) {
            throw std::runtime_error("can't write " + temporary.string());
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("can't replace " + path.string() + ": " + error.message());
    }
}

SavedGame read_save_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("can't open " + path.string());
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        throw std::runtime_error("can't read " + path.string());
    }
    try {
        return parse_saved_game(text);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(path.string() + ": " + error.what());
    }
}

}  // namespace manifold
