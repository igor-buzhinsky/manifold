#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace manifold {

/// A spell that the party can cast. Each spell is an effect that lasts for a while, see PartyMagic.
enum class Spell : std::uint8_t {
    kCandlelight,  ///< A light at the player's eyes, see scene_lighting().
    kLevitate,     ///< No gravity: the player flies up and down, but still collides, see Game::update().
};

/// All spells, in the order of the enum.
inline constexpr std::array<Spell, 2> kAllSpells{Spell::kCandlelight, Spell::kLevitate};

/// The key of a spell, as saves store it, console commands take it and the spell file names it: `candlelight` or
/// `levitate`.
const char* spell_key(Spell spell);

/// The spell with the key spell_key(), or nothing if `key` is none.
std::optional<Spell> parse_spell_key(std::string_view key);

/// What a spell costs and how long it lasts, and its name, read from the spell file, see load_game_data().
struct SpellType {
    std::string name;       ///< What the screen shows, e.g. "Candlelight".
    float cost = 0.0f;      ///< How much mana casting it costs, 0 or more.
    float duration = 0.0f;  ///< How long it lasts after it is cast, in seconds of game time, positive.

    bool operator==(const SpellType&) const = default;
};

/// Mana of a new party by default, which is also its maximum, see MagicConfig::max_mana.
inline constexpr float kStartMana = 100.0f;

/// Settings of the party's magic: its mana and the spells, read from the data files, see load_game_data().
struct MagicConfig {
    /// Mana of a new party, which is also its maximum, positive. A loaded party keeps the maximum of its save.
    float max_mana = kStartMana;
    /// How much mana the party regains per second of game time, up to its maximum, 0 or more.
    float mana_regeneration = 1.0f;
    /// Each spell, in the order of kAllSpells.
    std::array<SpellType, kAllSpells.size()> spells{{
        {.name = "Candlelight", .cost = 5.0f, .duration = 60.0f},
        {.name = "Levitate", .cost = 10.0f, .duration = 60.0f},
    }};

    /// The settings of `spell`.
    const SpellType& spell(Spell spell) const { return spells.at(static_cast<std::size_t>(spell)); }
    SpellType& spell(Spell spell) { return spells.at(static_cast<std::size_t>(spell)); }

    bool operator==(const MagicConfig&) const = default;
};

/// A spell that is in effect.
struct ActiveSpell {
    Spell spell = Spell::kCandlelight;
    float remaining = 0.0f;  ///< Seconds of game time until it ends, positive.

    bool operator==(const ActiveSpell&) const = default;
};

/// The party's shared mana, and the spells in effect.
struct PartyMagic {
    float mana = kStartMana;      ///< From 0 to `max_mana`.
    float max_mana = kStartMana;  ///< Positive.
    /// The spells in effect, in the order they were first cast, each at most once.
    std::vector<ActiveSpell> spells;

    /// A party of a new game, with all mana of `config` and no spells in effect.
    static PartyMagic fresh(const MagicConfig& config);

    /// Casts a spell, if there is enough mana: takes its SpellType::cost, and lets it last for its
    /// SpellType::duration. A spell that is already in effect keeps its place in `spells`, and lasts for the duration
    /// from now: what was left of it is not added.
    ///
    /// \param spell  The spell.
    /// \param config Costs and durations of the spells.
    /// \return False, with nothing changed, if the party has less mana than the spell costs.
    bool cast(Spell spell, const MagicConfig& config = {});

    /// Advances by `dt` seconds of game time: regains `regeneration` mana per second, up to `max_mana`, and ends the
    /// spells that have no time left.
    void advance(float dt, float regeneration = MagicConfig{}.mana_regeneration);

    /// Whether `spell` is in effect.
    bool active(Spell spell) const;

    /// Sets the mana, e.g. as a cheat, clamped to [0, max_mana].
    void set_mana(float value);

    /// The part of `max_mana` that is left, from 0 to 1.
    float mana_fraction() const;

    bool operator==(const PartyMagic&) const = default;
};

}  // namespace manifold
