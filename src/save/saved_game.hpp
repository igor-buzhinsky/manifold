#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "world/door.hpp"

namespace manifold {

/// Which of the two saves: the game as it was at the last autosave or exit, or at the start of its level.
enum class SaveKind : std::uint8_t {
    kProgress,    ///< `save.json`: the level as it was, with LevelProgress, see Game::progress_save().
    kLevelStart,  ///< `save_level_start.json`: the start of the level, see Game::level_start_save().
};

/// The name of the file of a save in the game's directory for user data: `save.json` or `save_level_start.json`.
const char* save_file_name(SaveKind kind);

/// The player in a save made in the middle of a level, see Player.
struct SavedPlayer {
    glm::vec3 position{0.0f};             ///< The point between the feet, see Player::position().
    float yaw = 0.0f;                     ///< See Player::yaw(), within [0, 2 pi).
    float pitch = 0.0f;                   ///< See Player::pitch(), within [-Player::kMaxPitch, Player::kMaxPitch].
    float vertical_speed = 0.0f;          ///< See Player::vertical_speed().
    glm::vec2 horizontal_velocity{0.0f};  ///< See Player::horizontal_velocity().
    bool on_ground = false;               ///< See Player::on_ground().

    bool operator==(const SavedPlayer&) const = default;
};

/// What changed in a level since its start, for a save made in the middle of it, see SavedGame::progress.
struct LevelProgress {
    double time = 0.0;                   ///< Game time when the save was made, in seconds, see Game::time().
    std::vector<Character> party;        ///< The party when the save was made, see SavedGame::party.
    PartyMagic magic;                    ///< The party's mana and spells when the save was made, see SavedGame::magic.
    std::vector<std::string> inventory;  ///< The keys of the party's items, see SavedGame::inventory.
    SavedPlayer player;                  ///< Where the player was and how they moved.
    /// The state of each door of World::doors(), in the same order, see Game::doors().
    std::vector<DoorState> doors;
    /// The keys of the items of each chest of World::chests(), in the same order, see Game::chest_inventories().
    std::vector<std::vector<std::string>> chests;

    bool operator==(const LevelProgress&) const = default;
};

/// What a save holds: enough to start the saved level again, from its start, and, for a save made in the middle of the
/// level, what changed in it since then.
///
/// The level isn't stored but generated again from its seed, so a save made by another version of the game can
/// give another level, if level generation changed.
struct SavedGame {
    std::uint64_t seed = 0;  ///< The seed the level was generated from, see Game::seed().
    int level_number = 0;    ///< How many levels came before it, see Game::level_number().
    /// The layout if it was given rather than chosen from the seed, see generate_level(). Only a first level can
    /// have one, given on the command line.
    std::optional<LayoutKind> layout;
    double time = 0.0;  ///< Game time at the start of the level, in seconds, see Game::time().
    /// The party at the start of the level, see Game::party(). It isn't made again from the seed, so it is kept even
    /// if the number of portraits changes, though a character shows another portrait if the files of its portrait's
    /// key change.
    std::vector<Character> party;
    /// The party's mana and the spells in effect at the start of the level, see Game::magic().
    PartyMagic magic;
    /// The keys of the items that the party carried at the start of the level, in order, see Game::inventory() and
    /// ItemType::key. Keys, unlike ItemId, stay right when items are added to the item file.
    // Without the initializer, GCC warns about saves that leave it out.
    std::vector<std::string> inventory = {};  // NOLINT(readability-redundant-member-init)
    /// For a save made in the middle of the level, see SaveKind::kProgress, the state of the level and the party when
    /// it was made, which Game::load() restores after starting the level. A save without it, such as one made by an
    /// older version of the game, starts the level from its start.
    // Without the initializer, GCC warns about saves that leave it out.
    std::optional<LevelProgress> progress = std::nullopt;  // NOLINT(readability-redundant-member-init)

    bool operator==(const SavedGame&) const = default;
};

/// The version of the save format that save_to_json() writes and parse_saved_game() reads.
///
/// Increase it when the meaning of the fields changes, e.g. when level generation from a seed changes, so older
/// saves are refused instead of giving other levels.
inline constexpr int kSaveFormatVersion = 5;

/// Writes a save as JSON: an object with `version` (kSaveFormatVersion), `seed`, `level_number`, `time`, `party`,
/// `mana`, `max_mana`, `spells`, `inventory`, only if the layout was given, `layout` with its name, see
/// layout_name(), and only if the save has SavedGame::progress, `progress`.
///
/// `party` is an array with an object for each character: `portrait` (null for none, otherwise an object with the
/// `key` of the portrait, see asset_key_name(), and its `index` among the files of the key), `gender` (see
/// gender_name()), `hp` and `max_hp`. `spells` is an array with an object for each spell in effect, in the order of
/// PartyMagic::spells: `spell` (see spell_key()) and `remaining` (seconds). `inventory` is an array of the keys of
/// the party's items, see SavedGame::inventory.
///
/// `progress` is an object with `time`, `party`, `mana`, `max_mana`, `spells` and `inventory` as above, `player` (an
/// object with `position` [x, y, z], `yaw`, `pitch`, `vertical_speed`, `horizontal_velocity` [x, y] and `on_ground`),
/// `doors` (an array with an object of `openness` and `target` for each door) and `chests` (an array with an array of
/// item keys for each chest).
std::string save_to_json(const SavedGame& save);

/// Reads a save written by save_to_json().
///
/// \throws std::runtime_error if `json` is not valid JSON, has another version, or lacks a field or has a wrong one,
///         such as a negative level number or an unknown layout. A party must have at least one character, and each
///         a positive `max_hp`, an `hp` from 0 to it, and a portrait that is null or of a portrait key with a
///         non-negative index, which may be beyond the files of the key, see AssetMapping::path(). `max_mana`
///         must be positive and `mana` from 0 to it. Each spell must be known, appear at most once, and have a
///         positive `remaining` time, which Game::load() cuts to the spell's duration. `inventory` must be an array of
///         strings; a save without it, made before the party had items, has none. `progress` is optional, but if it
///         is there, it must have all its fields, with the same checks, a `time` not before the level's, finite
///         numbers for the player, a `pitch` within the limits of Player::pitch() and a `yaw` within [0, 2 pi), and
///         doors with an `openness` from 0 to 1 and a `target` of 0 or 1. Unknown fields are ignored.
SavedGame parse_saved_game(std::string_view json);

/// Writes a save file, replacing the old one.
///
/// The file is first written next to `path`, with `.tmp` appended, and then renamed, so a game that stops while
/// saving keeps the old save.
///
/// \throws std::runtime_error if the file can't be written.
void write_save_file(const std::filesystem::path& path, const SavedGame& save);

/// Reads a save file written by write_save_file().
///
/// \throws std::runtime_error if the file can't be read, e.g. if it doesn't exist, or if parse_saved_game() fails.
SavedGame read_save_file(const std::filesystem::path& path);

}  // namespace manifold
