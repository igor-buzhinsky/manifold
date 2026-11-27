#pragma once

#include <filesystem>
#include <stdexcept>
#include <string_view>

#include "game.hpp"
#include "level/layout_factory.hpp"
#include "party/spells.hpp"
#include "render/lighting.hpp"

namespace manifold {

// The settings of the game that are read from the files of its data directory (see kDataDirName), so that they can be
// tuned without building the game again. Unlike the items (see ItemCatalog), these files must be complete and right:
// every field must be there, with a value of the right type and range, and no field may be unknown, so that a typo
// can't leave a value unchanged unnoticed. The structs keep defaults in code, which tests use; the game and the
// movement simulation use the files.

/// Name of the file of the spells: their names, costs and durations, see parse_spells().
inline constexpr std::string_view kSpellsFileName = "spells.json";
/// Name of the file of the settings of the level layouts, see parse_level_layouts().
inline constexpr std::string_view kLevelLayoutsFileName = "level_layouts.json";
/// Name of the file of the other constants: of the player, the party, the objects of levels and the lights, see
/// parse_constants().
inline constexpr std::string_view kConstantsFileName = "constants.json";

/// A data file that is missing, isn't valid JSON, or has a field that is missing, unknown or wrong. Its message lists
/// all problems found, one per line, each with the file.
class GameDataError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Reads the spells from the text of kSpellsFileName into `magic`: an object with a field for each spell, named by
/// spell_key(), whose value is an object with `name` (a string that isn't empty), `cost` (mana, 0 or more) and
/// `duration` (seconds, positive), e.g. `{"candlelight": {"name": "Candlelight", "cost": 5, "duration": 60}, ...}`.
///
/// \param source Where the text comes from, e.g. the file's path, for the messages.
/// \throws GameDataError if a spell or a field is missing, unknown or wrong. Then `magic` may be partly changed.
void parse_spells(std::string_view json, std::string_view source, MagicConfig& magic);

/// Reads the settings of the layouts from the text of kLevelLayoutsFileName into `level`: an object with `rooms` and
/// `debug`, each an object with `probability` (see LevelConfig) and the fields of RoomLayoutConfig or
/// DebugLayoutConfig by their names, e.g. `{"rooms": {"probability": 1, "grid_width": 80, ...}, "debug": {...}}`.
///
/// Besides the type and range of each field, it checks that the smallest of a range isn't above its largest (such as
/// `min_holes` and `max_holes`), that `min_ramps` isn't above `min_holes`, that the debug level isn't empty, and that
/// not all probabilities are 0. load_game_data() also generates a level of each layout, which finds the rest.
///
/// \throws GameDataError if a field is missing, unknown or wrong. Then `level` may be partly changed.
void parse_level_layouts(std::string_view json, std::string_view source, LevelConfig& level);

/// Reads the other constants from the text of kConstantsFileName into `game` and `lights`: an object with
/// - `player`: `radius` and `height` (metres, see PlayerParams), `walk_speed` and `run_speed` (metres per second),
///   `turn_speed` and `look_speed` (degrees per second, unlike GameConfig), `jump_height` (metres) and
///   `max_snap_slope`, see GameConfig;
/// - `party`: `size` and `max_hp`, see PartyConfig, and `max_mana` and `mana_regeneration` (per second), see
///   MagicConfig;
/// - `level_objects`: `torch_flames`, an object with the weights `orange`, `blue` and `green`, and
///   `golden_chest_chance`, see LevelObjectChances, and `wooden_chest` and `golden_chest`, each an object with the
///   fields of ContainerLoot, see ChestLoot;
/// - `lighting`: `candlelight` with `color` (an array of 3 numbers, linear RGB), `intensity` and `range`, the
///   player's light of LightSettings; `torch` and `exit`, each with `intensity` and `range`; `fade_distance` and
///   `level_ambient_luminance`.
///
/// Besides the type and range of each field, it checks that the smallest of a range of the loot isn't above its
/// largest, and that not all torch flames have a weight of 0. A loot whose largest total is below its dearest item
/// only gets a warning, see check_container_loot().
///
/// \throws GameDataError if a field is missing, unknown or wrong. Then `game` and `lights` may be partly changed.
void parse_constants(std::string_view json, std::string_view source, GameConfig& game, LightSettings& lights);

/// The settings of the game from its data files, see load_game_data().
struct GameData {
    /// Settings of a new game. Its `level.assets` are not set, since they come from the asset mapping.
    GameConfig game;
    LightSettings lights;  ///< The lights as they are at the start, before the debug overlay changes them.
};

/// Reads all data files of `data_dir`: the items (see ItemCatalog::load(), which only warns about problems),
/// kSpellsFileName, kLevelLayoutsFileName and kConstantsFileName. Then it checks that each layout can generate a level,
/// with the seed 1 and the player's radius as the clearance, since some settings only fail together, such as tiles too
/// small for the player.
///
/// \throws GameDataError with all problems of all files if one is missing or has a problem, or a layout can't generate
///         a level.
GameData load_game_data(const std::filesystem::path& data_dir);

}  // namespace manifold
