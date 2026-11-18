#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace manifold {

/// The gender of a character, which chooses the portraits it can have, see find_portraits().
enum class Gender : std::uint8_t {
    kMale,
    kFemale,
};

/// The name of a gender, as saves store it and the `party` command shows it: `male` or `female`.
const char* gender_name(Gender gender);

/// The gender named by gender_name(), or nothing if `name` names none.
std::optional<Gender> parse_gender_name(std::string_view name);

/// A member of the party.
struct Character {
    /// The portrait's file, relative to the directory of portraits with `/` between directories, as
    /// PortraitFile::path, or empty if the character has none, see make_party().
    std::string portrait;
    Gender gender = Gender::kMale;
    int hp = 0;      ///< Current hit points, from 0 to `max_hp`.
    int max_hp = 0;  ///< Hit points when unhurt, positive.

    /// The part of `max_hp` that is left, from 0 to 1. A character without `max_hp` has none left.
    float hp_fraction() const;

    bool operator==(const Character&) const = default;
};

/// A portrait that characters can have.
struct PortraitFile {
    /// The file, relative to the directory of portraits with `/` between directories, e.g.
    /// `male/metmuseum-org/11058-healy-portrait-of-the-artist.png`.
    std::string path;
    /// The gender of the characters it is for, or nothing for any gender, e.g. for a skeleton.
    std::optional<Gender> gender;

    bool operator==(const PortraitFile&) const = default;
};

/// Settings for a new party.
struct PartyConfig {
    std::size_t size = 3;  ///< How many characters it has.
    int max_hp = 100;      ///< Hit points of each character, positive.
};

/// Names of the directories of portraits for each group: men, women and anyone.
inline constexpr std::string_view kMalePortraitsDir = "male";
inline constexpr std::string_view kFemalePortraitsDir = "female";
inline constexpr std::string_view kAnyPortraitsDir = "any";

/// Finds the portraits: the PNG files in `root/male`, `root/female` and `root/any`, at any depth, see
/// find_asset_files(). Files elsewhere under `root` are not used.
///
/// \return The portraits, sorted by path, so that the same files always give the same order. Empty if there are none.
std::vector<PortraitFile> find_portraits(const std::filesystem::path& root);

/// Makes a party of new, unhurt characters, each with another portrait chosen at random.
///
/// The choice depends only on `seed` and the portraits. Each character gets the gender of its portrait, or a random one
/// for a portrait of any gender. If there are fewer portraits than characters, a warning is logged, and the last
/// characters get no portrait and a random gender.
///
/// \param portraits The portraits to choose from, e.g. from find_portraits().
/// \param config    How many characters to make, and their hit points.
/// \param seed      Seed of the random choices.
std::vector<Character> make_party(std::span<const PortraitFile> portraits, const PartyConfig& config,
                                  std::uint64_t seed);

}  // namespace manifold
