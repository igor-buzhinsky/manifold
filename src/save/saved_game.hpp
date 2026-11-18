#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "level/level.hpp"
#include "party/party.hpp"

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
    /// if the portraits change.
    std::vector<Character> party;

    bool operator==(const SavedGame&) const = default;
};

/// The version of the save format that save_to_json() writes and parse_saved_game() reads.
///
/// Increase it when the meaning of the fields changes, e.g. when level generation from a seed changes, so older
/// saves are refused instead of giving other levels.
inline constexpr int kSaveFormatVersion = 2;

/// Writes a save as JSON: an object with `version` (kSaveFormatVersion), `seed`, `level_number`, `time`, `party` and,
/// only if the layout was given, `layout` with its name, see layout_name().
///
/// `party` is an array with an object for each character: `portrait` (an empty string for none), `gender` (see
/// gender_name()), `hp` and `max_hp`.
std::string save_to_json(const SavedGame& save);

/// Reads a save written by save_to_json().
///
/// \throws std::runtime_error if `json` is not valid JSON, has another version, or lacks a field or has a wrong one,
///         such as a negative level number or an unknown layout. A party must have at least one character, and each
///         a positive `max_hp`, an `hp` from 0 to it, and a portrait that is a relative path without `..`. Unknown
///         fields are ignored.
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
