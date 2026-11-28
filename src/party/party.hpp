#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {

/// The gender of a character, which chooses the portraits it can have, see make_party().
enum class Gender : std::uint8_t {
    kMale,
    kFemale,
};

/// The name of a gender, as saves store it and the `party` command shows it: `male` or `female`.
const char* gender_name(Gender gender);

/// The gender named by gender_name(), or nothing if `name` names none.
std::optional<Gender> parse_gender_name(std::string_view name);

/// A portrait of a character: one of the files of a portrait key in the asset mapping.
struct Portrait {
    /// AssetKey::kMalePortraits, AssetKey::kFemalePortraits or AssetKey::kAnyPortraits.
    AssetKey key = AssetKey::kAnyPortraits;
    AssetIndex index = 0;  ///< Which file of the key.

    bool operator==(const Portrait&) const = default;
};

/// Whether `key` is one of portraits: AssetKey::kMalePortraits, AssetKey::kFemalePortraits or AssetKey::kAnyPortraits.
bool is_portrait_key(AssetKey key);

/// A member of the party.
struct Character {
    /// The portrait, or nothing if the character has none, see make_party().
    std::optional<Portrait> portrait;
    Gender gender = Gender::kMale;
    int hp = 0;      ///< Current hit points, from 0 to `max_hp`.
    int max_hp = 0;  ///< Hit points when unhurt, positive.

    /// The part of `max_hp` that is left, from 0 to 1. A character without `max_hp` has none left.
    float hp_fraction() const;
    /// Whether the character is dead: it has no hit points left. Its portrait is then gray, and once all characters of
    /// the party are dead, the game is over, see party_defeated().
    bool dead() const { return hp <= 0; }

    bool operator==(const Character&) const = default;
};

/// Whether all characters of `party` are dead, see Character::dead(), so that the game is over. A party without
/// characters isn't.
bool party_defeated(std::span<const Character> party);

/// Settings for a new party.
struct PartyConfig {
    std::size_t size = 3;  ///< How many characters it has.
    int max_hp = 100;      ///< Hit points of each character, positive.
};

/// Makes a party of new, unhurt characters, each with another portrait chosen at random among the files of the
/// portrait keys of the asset mapping.
///
/// The choice depends only on `seed` and how many files the portrait keys have. Each character gets the gender of its
/// portrait's key, or a random one for AssetKey::kAnyPortraits. If there are fewer portraits than characters, a warning
/// is logged, and the last characters get no portrait and a random gender.
///
/// \param portraits How many files each key has, see AssetMapping::counts(). Only the portrait keys are used.
/// \param config    How many characters to make, and their hit points.
/// \param seed      Seed of the random choices.
std::vector<Character> make_party(const AssetCounts& portraits, const PartyConfig& config, std::uint64_t seed);

}  // namespace manifold
