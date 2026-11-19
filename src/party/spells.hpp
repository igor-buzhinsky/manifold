#pragma once

#include <array>
#include <cstdint>
#include <optional>
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

/// The name of a spell, as saves store it and console commands take it: `candlelight` or `levitate`.
const char* spell_name(Spell spell);

/// The name of a spell as the screen shows it: `Candlelight` or `Levitate`.
const char* spell_title(Spell spell);

/// The spell named by spell_name(), or nothing if `name` names none.
std::optional<Spell> parse_spell_name(std::string_view name);

/// How much mana casting a spell costs: 5 for Candlelight, 10 for Levitate.
float spell_cost(Spell spell);

/// How long a spell lasts after it is cast, in seconds of game time: 60 for every spell.
float spell_duration(Spell spell);

/// A spell that is in effect.
struct ActiveSpell {
    Spell spell = Spell::kCandlelight;
    float remaining = 0.0f;  ///< Seconds of game time until it ends, positive.

    bool operator==(const ActiveSpell&) const = default;
};

/// Mana of a new party, which is also its maximum.
inline constexpr float kStartMana = 100.0f;
/// How much mana the party regains per second of game time, up to its maximum.
inline constexpr float kManaRegeneration = 1.0f;

/// The party's shared mana, and the spells in effect.
struct PartyMagic {
    float mana = kStartMana;      ///< From 0 to `max_mana`.
    float max_mana = kStartMana;  ///< Positive.
    /// The spells in effect, in the order they were first cast, each at most once.
    std::vector<ActiveSpell> spells;

    /// Casts a spell, if there is enough mana: takes its spell_cost(), and lets it last for spell_duration(). A spell
    /// that is already in effect keeps its place in `spells`, and lasts for spell_duration() from now: what was left of
    /// it is not added.
    ///
    /// \return False, with nothing changed, if the party has less mana than the spell costs.
    bool cast(Spell spell);

    /// Advances by `dt` seconds of game time: regains kManaRegeneration per second, up to `max_mana`, and ends the
    /// spells that have no time left.
    void advance(float dt);

    /// Whether `spell` is in effect.
    bool active(Spell spell) const;

    /// Sets the mana, e.g. as a cheat, clamped to [0, max_mana].
    void set_mana(float value);

    /// The part of `max_mana` that is left, from 0 to 1.
    float mana_fraction() const;

    bool operator==(const PartyMagic&) const = default;
};

}  // namespace manifold
