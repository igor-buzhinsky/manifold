#include "save/saved_game.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "level/layout_factory.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/text_file.hpp"

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

/// Whether `value` is an integer from `min` to `max`.
bool is_int_in(const nlohmann::json& value, std::int64_t min, std::int64_t max) {
    if (!value.is_number_integer()) {
        return false;
    }
    // A large unsigned number doesn't fit into std::int64_t, but is out of range anyway.
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(max)) {
        return false;
    }
    const auto number = value.get<std::int64_t>();
    return number >= min && number <= max;
}

/// Reads the portrait of a character of a save: null for none, or an object with a portrait `key` and an `index`.
///
/// \throws std::runtime_error if it is something else.
std::optional<Portrait> parse_portrait(const nlohmann::json& value) {
    if (value.is_null()) {
        return std::nullopt;
    }
    constexpr const char* kExpected = "null or an object with a portrait key and an index";
    if (!value.is_object()) {
        throw_invalid("portrait", value, kExpected);
    }
    const auto key_field = value.find("key");
    const auto index_field = value.find("index");
    if (key_field == value.end() || !key_field->is_string() || index_field == value.end() ||
        !is_int_in(*index_field, 0, std::numeric_limits<AssetIndex>::max())) {
        throw_invalid("portrait", value, kExpected);
    }
    const std::optional<AssetKey> key = parse_asset_key_name(key_field->get<std::string>());
    if (!key || !is_portrait_key(*key)) {
        throw_invalid("portrait", value, kExpected);
    }
    return Portrait{.key = *key, .index = index_field->get<AssetIndex>()};
}

/// Reads a character of the party of a save.
///
/// \throws std::runtime_error if it lacks a field or has a wrong one.
Character parse_character(const nlohmann::json& object) {
    if (!object.is_object()) {
        throw_invalid("character", object, "an object");
    }
    Character character;
    character.portrait = parse_portrait(field(object, "portrait"));

    const nlohmann::json& gender = field(object, "gender");
    const std::optional<Gender> parsed =
        gender.is_string() ? parse_gender_name(gender.get<std::string>()) : std::nullopt;
    if (!parsed) {
        throw_invalid("gender", gender, "male or female");
    }
    character.gender = *parsed;

    const nlohmann::json& max_hp = field(object, "max_hp");
    if (!is_int_in(max_hp, 1, std::numeric_limits<int>::max())) {
        throw_invalid("max_hp", max_hp, "a positive integer");
    }
    character.max_hp = max_hp.get<int>();

    const nlohmann::json& hp = field(object, "hp");
    if (!is_int_in(hp, 0, character.max_hp)) {
        throw_invalid("hp", hp, "an integer from 0 to max_hp");
    }
    character.hp = hp.get<int>();
    return character;
}

/// Whether `value` is a finite number from `min` to `max`.
bool is_number_in(const nlohmann::json& value, double min, double max) {
    if (!value.is_number()) {
        return false;
    }
    const auto number = value.get<double>();
    return std::isfinite(number) && number >= min && number <= max;
}

/// Reads the party's mana and the spells in effect from a save.
///
/// \throws std::runtime_error if a field is missing or wrong.
PartyMagic parse_magic(const nlohmann::json& object) {
    PartyMagic magic;
    const nlohmann::json& max_mana = field(object, "max_mana");
    if (!is_number_in(max_mana, 0.0, std::numeric_limits<float>::max()) || max_mana.get<double>() <= 0.0) {
        throw_invalid("max_mana", max_mana, "a positive number");
    }
    magic.max_mana = max_mana.get<float>();

    const nlohmann::json& mana = field(object, "mana");
    if (!is_number_in(mana, 0.0, magic.max_mana)) {
        throw_invalid("mana", mana, "a number from 0 to max_mana");
    }
    magic.mana = mana.get<float>();

    const nlohmann::json& spells = field(object, "spells");
    if (!spells.is_array()) {
        throw_invalid("spells", spells, "an array");
    }
    for (const nlohmann::json& active : spells) {
        if (!active.is_object()) {
            throw_invalid("spell in effect", active, "an object");
        }
        const nlohmann::json& name = field(active, "spell");
        const std::optional<Spell> spell = name.is_string() ? parse_spell_name(name.get<std::string>()) : std::nullopt;
        if (!spell) {
            std::string names;
            for (const Spell known : kAllSpells) {
                names += std::string(names.empty() ? "" : " or ") + spell_name(known);
            }
            throw_invalid("spell", name, names.c_str());
        }
        if (magic.active(*spell)) {
            throw_invalid("spell", name, "each spell at most once");
        }
        const nlohmann::json& remaining = field(active, "remaining");
        if (!is_number_in(remaining, 0.0, spell_duration(*spell)) || remaining.get<double>() <= 0.0) {
            throw_invalid("remaining", remaining, "a positive number of seconds, at most the spell's duration");
        }
        magic.spells.push_back({.spell = *spell, .remaining = remaining.get<float>()});
    }
    return magic;
}

}  // namespace

std::string save_to_json(const SavedGame& save) {
    nlohmann::ordered_json json = {
        {"version", kSaveFormatVersion},
        {"seed", save.seed},
        {"level_number", save.level_number},
        {"time", save.time},
    };
    nlohmann::ordered_json party = nlohmann::ordered_json::array();
    for (const Character& character : save.party) {
        nlohmann::ordered_json portrait = nullptr;
        if (character.portrait) {
            portrait = {{"key", std::string(asset_key_name(character.portrait->key))},
                        {"index", character.portrait->index}};
        }
        party.push_back({
            {"portrait", std::move(portrait)},
            {"gender", gender_name(character.gender)},
            {"hp", character.hp},
            {"max_hp", character.max_hp},
        });
    }
    json["party"] = std::move(party);
    json["mana"] = save.magic.mana;
    json["max_mana"] = save.magic.max_mana;
    nlohmann::ordered_json spells = nlohmann::ordered_json::array();
    for (const ActiveSpell& active : save.magic.spells) {
        spells.push_back({{"spell", spell_name(active.spell)}, {"remaining", active.remaining}});
    }
    json["spells"] = std::move(spells);
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

    const nlohmann::json& party = field(object, "party");
    if (!party.is_array() || party.empty()) {
        throw_invalid("party", party, "an array of at least one character");
    }
    for (const nlohmann::json& character : party) {
        save.party.push_back(parse_character(character));
    }
    save.magic = parse_magic(object);

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
    const std::string text = read_text_file(path);
    try {
        return parse_saved_game(text);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(path.string() + ": " + error.what());
    }
}

}  // namespace manifold
