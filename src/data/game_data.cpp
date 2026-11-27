#include "data/game_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "game.hpp"
#include "items/item_catalog.hpp"
#include "items/loot.hpp"
#include "level/debug_layout.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "level/room_layout.hpp"
#include "party/spells.hpp"
#include "render/lighting.hpp"
#include "util/log.hpp"
#include "util/text_file.hpp"
#include "world/torch.hpp"

namespace manifold {

namespace {

using Json = nlohmann::ordered_json;

constexpr double kFloatMax = std::numeric_limits<float>::max();
constexpr double kIntMax = std::numeric_limits<int>::max();

/// The numbers that a field may have. Only a range without a largest number can leave out its smallest.
struct Range {
    double min = 0.0;
    double max = kFloatMax;
    bool above_min = false;  ///< Whether `min` itself is left out.
};

/// Any finite number.
constexpr Range kFinite{.min = -kFloatMax, .max = kFloatMax};
constexpr Range kNonNegative{.min = 0.0, .max = kFloatMax};
constexpr Range kPositive{.min = 0.0, .max = kFloatMax, .above_min = true};
/// A chance, from 0 to 1.
constexpr Range kFraction{.min = 0.0, .max = 1.0};
/// A number of things or of attempts: an integer from 0 to 1000, or 1000000 for attempts. The limits keep huge
/// values, such as a typo with too many zeros, from taking all memory or time.
constexpr Range kSmallCount{.min = 0.0, .max = 1000.0};
/// Like kSmallCount, but at least 1.
constexpr Range kPositiveSmallCount{.min = 1.0, .max = 1000.0};
constexpr Range kAttempts{.min = 1.0, .max = 1e6};
/// A positive integer, e.g. hit points or the value of an item.
constexpr Range kCount{.min = 1.0, .max = kIntMax};
/// Range of a light, at least 0.1 m: shadow maps need it beyond their near plane, see kShadowNearPlane.
constexpr Range kLightRange{.min = 0.1, .max = kFloatMax};
/// Most characters of a party: as many as the HUD has room for, see layout_party_hud().
constexpr Range kPartySize{.min = 1.0, .max = 6.0};

/// What a field with numbers in `range` must be, for messages, e.g. "a number from 0 to 1".
std::string describe(const Range& range, bool integer) {
    const char* kind = integer ? "an integer" : "a number";
    if (range.max >= kIntMax && range.min > -kIntMax) {
        if (range.above_min && range.min == 0.0) {
            return integer ? "a positive integer" : "a positive number";
        }
        return std::format("{} {} {:g}", kind, range.above_min ? "above" : "of", range.min) +
               (range.above_min ? "" : " or more");
    }
    return std::format("{} from {:g} to {:g}", kind, range.min, range.max);
}

/// Reads the fields of an object of a data file, and collects what is wrong with them as messages, see
/// GameDataError. Every field must be read once, and finish() reports the fields that weren't, as unknown.
class FieldReader {
public:
    /// \param object   The object, or null if it is missing or wrong, which a problem says already: then nothing is
    ///                 read and nothing more is reported.
    /// \param path     The name of the object in messages, e.g. `rooms` or `level_objects.wooden_chest`, or empty for
    ///                 the whole file.
    /// \param source   The file, for messages.
    /// \param problems Where the messages are added.
    FieldReader(const Json* object, std::string path, std::string_view source, std::vector<std::string>& problems)
        : object_(object), path_(std::move(path)), source_(source), problems_(&problems) {}

    /// Reads the field `name` into `out`, if it is a number in `range` (an integer for integral types).
    template <typename T>
    void number(const char* name, T& out, Range range) {
        const Json* value = find(name);
        if (value == nullptr) {
            return;
        }
        constexpr bool kInteger = std::is_integral_v<T>;
        if constexpr (kInteger) {
            range.min = std::max(range.min, -kIntMax);
            range.max = std::min(range.max, kIntMax);
        }
        const bool is_number = kInteger ? value->is_number_integer() : value->is_number();
        // A float can't hold every double: a tiny positive number would become 0, so the stored value is checked.
        double number = 0.0;
        if (is_number) {
            number = kInteger ? value->get<double>() : static_cast<double>(static_cast<T>(value->get<double>()));
        }
        const bool valid = is_number && std::isfinite(number) && number >= range.min && number <= range.max &&
                           (!range.above_min || number != range.min);
        if (!valid) {
            wrong(name, *value, describe(range, kInteger));
            return;
        }
        if constexpr (kInteger) {
            out = static_cast<T>(value->get<std::int64_t>());
        } else {
            out = static_cast<T>(number);
        }
    }

    /// Reads the field `name` into `out`, if it is a string that isn't empty.
    void text(const char* name, std::string& out) {
        const Json* value = find(name);
        if (value == nullptr) {
            return;
        }
        if (!value->is_string() || value->get<std::string>().empty()) {
            wrong(name, *value, "a string that isn't empty");
            return;
        }
        out = value->get<std::string>();
    }

    /// Reads the field `name` into `out`, if it is an array of 3 numbers of 0 or more, a linear RGB color.
    void color(const char* name, glm::vec3& out) {
        const Json* value = find(name);
        if (value == nullptr) {
            return;
        }
        const auto component = [](const Json& part) {
            return part.is_number() && std::isfinite(part.get<double>()) && part.get<double>() >= 0.0 &&
                   part.get<double>() <= kFloatMax;
        };
        if (!value->is_array() || value->size() != 3 || !std::ranges::all_of(*value, component)) {
            wrong(name, *value, "an array of 3 numbers of 0 or more");
            return;
        }
        out = {value->at(0).get<float>(), value->at(1).get<float>(), value->at(2).get<float>()};
    }

    /// A reader of the field `name`, which must be an object.
    FieldReader object(const char* name) {
        const Json* value = find(name);
        if (value != nullptr && !value->is_object()) {
            wrong(name, *value, "an object");
            value = nullptr;
        }
        return {value, field_path(name), source_, *problems_};
    }

    /// Reports a problem unless `low` is at most `high` (below it if `strict`), two fields of this object, once both
    /// were read right.
    void order(const char* low_name, double low, const char* high_name, double high, bool strict = false) {
        if (object_ == nullptr || !fields_right_ || low < high || (!strict && low == high)) {
            return;
        }
        add(std::format("{} {:g} is {} {} {:g}", field_path(low_name), low, strict ? "not below" : "above",
                        field_path(high_name), high));
    }

    /// Reports a problem with this object, e.g. a check of several fields, once all were read right.
    void check(bool valid, std::string_view message) {
        if (object_ != nullptr && fields_right_ && !valid) {
            add(std::format("{} {}", path_.empty() ? "the file" : path_, message));
        }
    }

    /// Reports the fields that weren't read as unknown.
    void finish() {
        if (object_ == nullptr) {
            return;
        }
        for (const auto& [name, value] : object_->items()) {
            if (!read_.contains(name)) {
                add(std::format("{} is unknown", field_path(name)));
            }
        }
    }

private:
    /// The field `name`, which counts as read, or null if it is missing, which is reported.
    const Json* find(const char* name) {
        if (object_ == nullptr) {
            return nullptr;
        }
        read_.insert(name);
        const auto found = object_->find(name);
        if (found == object_->end()) {
            add(std::format("{} is missing", field_path(name)));
            return nullptr;
        }
        return &*found;
    }

    void wrong(const char* name, const Json& value, std::string_view expected) {
        add(std::format("{} is {}, not {}", field_path(name), value.dump(), expected));
    }

    void add(std::string message) {
        fields_right_ = false;
        problems_->push_back(std::format("{}: {}", source_, message));
    }

    std::string field_path(std::string_view name) const {
        return path_.empty() ? std::string(name) : std::format("{}.{}", path_, name);
    }

    const Json* object_;
    std::string path_;
    std::string_view source_;
    std::vector<std::string>* problems_;
    std::set<std::string, std::less<>> read_;
    bool fields_right_ = true;  ///< Whether no problem was found in this object yet.
};

/// Parses the text of a data file, which must be a JSON object, and adds a problem if it isn't.
///
/// \return The object, or null (a JSON null) if it isn't one.
Json parse_object(std::string_view json, std::string_view source, std::vector<std::string>& problems) {
    try {
        Json object = Json::parse(json);
        if (object.is_object()) {
            return object;
        }
        problems.push_back(std::format("{}: the file is not a JSON object", source));
    } catch (const Json::parse_error& error) {
        problems.push_back(std::format("{}: the file is not valid JSON: {}", source, error.what()));
    }
    return nullptr;
}

/// A reader of the whole file `object`, see parse_object().
FieldReader file_reader(const Json& object, std::string_view source, std::vector<std::string>& problems) {
    return {object.is_object() ? &object : nullptr, "", source, problems};
}

/// Throws GameDataError with the problems, one per line, if there are any.
void throw_problems(const std::vector<std::string>& problems) {
    if (problems.empty()) {
        return;
    }
    std::string message;
    for (const std::string& problem : problems) {
        message += (message.empty() ? "" : "\n") + problem;
    }
    throw GameDataError(message);
}

void read_rooms(FieldReader& reader, RoomLayoutConfig& rooms) {
    reader.number("grid_width", rooms.grid_width, kPositiveSmallCount);
    reader.number("grid_height", rooms.grid_height, kPositiveSmallCount);
    reader.number("tile_size", rooms.tile_size, kPositive);
    reader.number("height", rooms.height, kPositive);
    reader.number("min_room_size", rooms.min_room_size, kPositiveSmallCount);
    reader.number("max_room_size", rooms.max_room_size, kPositiveSmallCount);
    reader.number("placement_attempts", rooms.placement_attempts, kAttempts);
    reader.number("layer_count", rooms.layer_count, kPositiveSmallCount);
    reader.number("slab_thickness", rooms.slab_thickness, kPositive);
    reader.number("hole_length", rooms.hole_length, kPositiveSmallCount);
    reader.number("min_holes", rooms.min_holes, kSmallCount);
    reader.number("max_holes", rooms.max_holes, kSmallCount);
    reader.number("min_ramps", rooms.min_ramps, kSmallCount);
    reader.number("max_ramps", rooms.max_ramps, kSmallCount);
    reader.number("max_plan_attempts", rooms.max_plan_attempts, kAttempts);
    reader.number("min_torch_chance", rooms.min_torch_chance, kFraction);
    reader.number("max_torch_chance", rooms.max_torch_chance, kFraction);
    reader.number("torch_height", rooms.torch_height, kNonNegative);
    reader.number("min_door_chance", rooms.min_door_chance, kFraction);
    reader.number("max_door_chance", rooms.max_door_chance, kFraction);
    reader.number("chest_chance", rooms.chest_chance, kFraction);
    reader.order("min_room_size", rooms.min_room_size, "max_room_size", rooms.max_room_size);
    reader.order("min_holes", rooms.min_holes, "max_holes", rooms.max_holes);
    reader.order("min_ramps", rooms.min_ramps, "max_ramps", rooms.max_ramps);
    reader.order("min_ramps", rooms.min_ramps, "min_holes", rooms.min_holes);
    reader.order("min_torch_chance", rooms.min_torch_chance, "max_torch_chance", rooms.max_torch_chance);
    reader.order("min_door_chance", rooms.min_door_chance, "max_door_chance", rooms.max_door_chance);
    // A room needs a tile for its wall on each side.
    reader.check(
        rooms.max_room_size + 2 <= std::min(rooms.grid_width, rooms.grid_height),
        std::format("has rooms of up to {} tiles, which don't fit with their walls into a grid of {} x {} tiles",
                    rooms.max_room_size, rooms.grid_width, rooms.grid_height));
}

void read_debug(FieldReader& reader, DebugLayoutConfig& debug) {
    reader.number("x_min", debug.x_min, kFinite);
    reader.number("x_max", debug.x_max, kFinite);
    reader.number("y_min", debug.y_min, kFinite);
    reader.number("y_max", debug.y_max, kFinite);
    reader.number("obstacle_count", debug.obstacle_count, kSmallCount);
    reader.number("obstacle_size", debug.obstacle_size, kPositive);
    reader.number("obstacle_height", debug.obstacle_height, kPositive);
    reader.number("ceiling_thickness", debug.ceiling_thickness, kPositive);
    reader.number("ramp_count", debug.ramp_count, kSmallCount);
    reader.number("ramp_length", debug.ramp_length, kPositive);
    reader.number("ramp_width", debug.ramp_width, kPositive);
    reader.number("max_attempts_per_block", debug.max_attempts_per_block, kAttempts);
    reader.number("torch_spacing", debug.torch_spacing, kPositive);
    reader.number("torch_height", debug.torch_height, kNonNegative);
    reader.number("torches_per_chest", debug.torches_per_chest, kSmallCount);
    reader.order("x_min", debug.x_min, "x_max", debug.x_max, true);
    reader.order("y_min", debug.y_min, "y_max", debug.y_max, true);
}

/// Reads how a kind of chest is filled, and warns about limits that can leave it empty, see check_container_loot().
void read_loot(FieldReader reader, ContainerLoot& loot, std::string_view name) {
    reader.number("min_draws", loot.min_draws, kSmallCount);
    reader.number("max_draws", loot.max_draws, kSmallCount);
    reader.number("min_item_value", loot.min_item_value, kNonNegative);
    reader.number("max_item_value", loot.max_item_value, kNonNegative);
    reader.number("max_total_value", loot.max_total_value, kNonNegative);
    reader.order("min_draws", loot.min_draws, "max_draws", loot.max_draws);
    reader.order("min_item_value", loot.min_item_value, "max_item_value", loot.max_item_value);
    reader.finish();
    if (loot.min_draws <= loot.max_draws && loot.min_item_value <= loot.max_item_value) {
        check_container_loot(loot, name);
    }
}

/// Reads how bright and how far a light shines.
template <typename Params>
void read_light(FieldReader reader, Params& params) {
    reader.number("intensity", params.intensity, kNonNegative);
    reader.number("range", params.range, kLightRange);
    reader.finish();
}

}  // namespace

void parse_spells(std::string_view json, std::string_view source, MagicConfig& magic) {
    std::vector<std::string> problems;
    const Json object = parse_object(json, source, problems);
    FieldReader file = file_reader(object, source, problems);
    for (const Spell spell : kAllSpells) {
        SpellType& type = magic.spell(spell);
        FieldReader reader = file.object(spell_key(spell));
        reader.text("name", type.name);
        reader.number("cost", type.cost, kNonNegative);
        reader.number("duration", type.duration, kPositive);
        reader.finish();
    }
    file.finish();
    throw_problems(problems);
}

void parse_level_layouts(std::string_view json, std::string_view source, LevelConfig& level) {
    std::vector<std::string> problems;
    const Json object = parse_object(json, source, problems);
    FieldReader file = file_reader(object, source, problems);

    FieldReader rooms = file.object("rooms");
    rooms.number("probability", level.rooms_probability, kNonNegative);
    read_rooms(rooms, level.rooms);
    rooms.finish();

    FieldReader debug = file.object("debug");
    debug.number("probability", level.debug_probability, kNonNegative);
    read_debug(debug, level.debug);
    debug.finish();

    file.check(level.rooms_probability > 0.0f || level.debug_probability > 0.0f,
               "has no layout with a positive probability");
    file.finish();
    throw_problems(problems);
}

void parse_constants(std::string_view json, std::string_view source, GameConfig& game, LightSettings& lights) {
    std::vector<std::string> problems;
    const Json object = parse_object(json, source, problems);
    FieldReader file = file_reader(object, source, problems);
    constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

    FieldReader player = file.object("player");
    player.number("radius", game.player.radius, kPositive);
    player.number("height", game.player.height, kPositive);
    player.number("walk_speed", game.walk_speed, kPositive);
    player.number("run_speed", game.run_speed, kPositive);
    float turn_degrees = game.turn_speed / kRadiansPerDegree;
    player.number("turn_speed", turn_degrees, kPositive);
    game.turn_speed = turn_degrees * kRadiansPerDegree;
    float look_degrees = game.look_speed / kRadiansPerDegree;
    player.number("look_speed", look_degrees, kPositive);
    game.look_speed = look_degrees * kRadiansPerDegree;
    player.number("jump_height", game.jump_height, kNonNegative);
    player.number("max_snap_slope", game.max_snap_slope, kNonNegative);
    player.order("walk_speed", game.walk_speed, "run_speed", game.run_speed);
    player.finish();

    FieldReader party = file.object("party");
    int size = static_cast<int>(game.party.size);
    party.number("size", size, kPartySize);
    game.party.size = static_cast<std::size_t>(size);
    party.number("max_hp", game.party.max_hp, kCount);
    party.number("max_mana", game.magic.max_mana, kPositive);
    party.number("mana_regeneration", game.magic.mana_regeneration, kNonNegative);
    party.finish();

    FieldReader objects = file.object("level_objects");
    LevelObjectChances& chances = game.level.object_chances;
    FieldReader flames = objects.object("torch_flames");
    for (const TorchFlame flame : {TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen}) {
        const std::string name(torch_flame_name(flame));
        flames.number(name.c_str(), chances.torch_flames.at(static_cast<std::size_t>(flame)), kNonNegative);
    }
    flames.check(std::ranges::any_of(chances.torch_flames, [](double weight) { return weight > 0.0; }),
                 "has no flame with a positive weight");
    flames.finish();
    objects.number("golden_chest_chance", chances.golden_chest, kFraction);
    read_loot(objects.object("wooden_chest"), game.level.chest_loot.wooden, "wooden chests");
    read_loot(objects.object("golden_chest"), game.level.chest_loot.golden, "golden chests");
    objects.finish();

    FieldReader lighting = file.object("lighting");
    FieldReader candlelight = lighting.object("candlelight");
    candlelight.color("color", lights.player_color);
    candlelight.number("intensity", lights.player_intensity, kNonNegative);
    candlelight.number("range", lights.player_range, kLightRange);
    candlelight.finish();
    read_light(lighting.object("torch"), lights.torch);
    read_light(lighting.object("exit"), lights.exit);
    lighting.number("fade_distance", lights.fade_distance, kNonNegative);
    lighting.number("level_ambient_luminance", lights.level_ambient_luminance, kNonNegative);
    lighting.finish();

    file.finish();
    throw_problems(problems);
}

GameData load_game_data(const std::filesystem::path& data_dir) {
    GameData data;
    data.game.level.items = ItemCatalog::load(data_dir);
    std::vector<std::string> problems;
    const auto read = [&data_dir, &problems](std::string_view name, const auto& parse) {
        const std::filesystem::path file = data_dir / name;
        try {
            parse(read_text_file(file), file.string());
        } catch (const GameDataError& error) {
            problems.emplace_back(error.what());
        } catch (const std::runtime_error& error) {
            problems.push_back(std::format("Can't read the game data: {}", error.what()));
        }
    };
    read(kSpellsFileName,
         [&data](std::string_view json, std::string_view source) { parse_spells(json, source, data.game.magic); });
    read(kLevelLayoutsFileName, [&data](std::string_view json, std::string_view source) {
        parse_level_layouts(json, source, data.game.level);
    });
    read(kConstantsFileName, [&data](std::string_view json, std::string_view source) {
        parse_constants(json, source, data.game, data.lights);
    });
    if (problems.empty()) {
        // Settings of several files that must fit together.
        const std::string constants = (data_dir / kConstantsFileName).string();
        const RoomLayoutConfig& rooms = data.game.level.rooms;
        if (data.game.player.height >= rooms.height) {
            problems.push_back(std::format("{}: player.height {:g} is not below rooms.height {:g} of {}", constants,
                                           data.game.player.height, rooms.height, kLevelLayoutsFileName));
        }
        for (const Spell spell : kAllSpells) {
            const SpellType& type = data.game.magic.spell(spell);
            if (type.cost > data.game.magic.max_mana) {
                problems.push_back(std::format("{}: party.max_mana {:g} is below {}.cost {:g} of {}", constants,
                                               data.game.magic.max_mana, spell_key(spell), type.cost, kSpellsFileName));
            }
        }
    }
    if (problems.empty()) {
        const std::string layouts = (data_dir / kLevelLayoutsFileName).string();
        for (const LayoutKind kind : {LayoutKind::kRooms, LayoutKind::kDebug}) {
            try {
                // The level is left out; only whether it can be made counts.
                static_cast<void>(generate_level(data.game.level, kind, data.game.player.radius, 1));
            } catch (const std::exception& error) {
                problems.push_back(
                    std::format("{}: the {} layout can't generate a level for a player of radius {:g} m: {}", layouts,
                                layout_name(kind), data.game.player.radius, error.what()));
            }
        }
    }
    throw_problems(problems);
    log_info("Game data {}: spells, level layouts and constants", data_dir.string());
    return data;
}

}  // namespace manifold
