#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"

namespace manifold {

/// What a save holds: enough to start the saved level again, from its start.
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

    bool operator==(const SavedGame&) const = default;
};

/// The version of the save format that save_to_json() writes and parse_saved_game() reads.
///
/// Increase it when the meaning of the fields changes, e.g. when level generation from a seed changes, so older
/// saves are refused instead of giving other levels.
inline constexpr int kSaveFormatVersion = 5;

/// Writes a save as JSON: an object with `version` (kSaveFormatVersion), `seed`, `level_number`, `time`, `party`,
/// `mana`, `max_mana`, `spells`, `inventory` and, only if the layout was given, `layout` with its name, see
/// layout_name().
///
/// `party` is an array with an object for each character: `portrait` (null for none, otherwise an object with the
/// `key` of the portrait, see asset_key_name(), and its `index` among the files of the key), `gender` (see
/// gender_name()), `hp` and `max_hp`. `spells` is an array with an object for each spell in effect, in the order of
/// PartyMagic::spells: `spell` (see spell_name()) and `remaining` (seconds). `inventory` is an array of the keys of
/// the party's items, see SavedGame::inventory.
std::string save_to_json(const SavedGame& save);

/// Reads a save written by save_to_json().
///
/// \throws std::runtime_error if `json` is not valid JSON, has another version, or lacks a field or has a wrong one,
///         such as a negative level number or an unknown layout. A party must have at least one character, and each
///         a positive `max_hp`, an `hp` from 0 to it, and a portrait that is null or of a portrait key with a
///         non-negative index, which may be beyond the files of the key, see AssetMapping::path(). `max_mana`
///         must be positive and `mana` from 0 to it. Each spell must be known, appear at most once, and have a
///         `remaining` time above 0 and at most its spell_duration(). `inventory` must be an array of strings; a save
///         without it, made before the party had items, has none. Unknown fields are ignored.
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
