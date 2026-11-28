#pragma once

#include <span>

#include "party/party.hpp"
#include "util/random.hpp"

namespace manifold {

/// How much a fall hurts the characters of the party, by the speed at which the player lands.
///
/// The damage grows with the kinetic energy of the impact above that of `safe_speed`: it is
/// kFullFallDamage * (v^2 - safe_speed^2) / (full_damage_speed^2 - safe_speed^2) for an impact speed v above
/// `safe_speed`, and 0 otherwise. kFullFallDamage are the hit points of an ordinary person, so the damage doesn't
/// depend on Character::max_hp. It depends on the speed rather than the height, so that the same fall hurts less where
/// gravity is weaker. Each character then gets its own part of it, see fall_damage().
///
/// The defaults are a bit more forgiving than real falls onto a hard floor: landing at 8 m/s, after a free fall of
/// 3.3 m under Earth gravity, as from one layer of rooms to the next, doesn't hurt, and one of 16.5 m, at 18 m/s, takes
/// all 100 hit points on average.
struct FallDamageConfig {
    /// The fastest landing that doesn't hurt, in metres per second.
    float safe_speed = 8.0f;
    /// The landing speed whose damage, before the spread, is kFullFallDamage, in metres per second. Faster than
    /// `safe_speed`.
    float full_damage_speed = 18.0f;
    /// How far the damage of each character may be from that of the speed, as a part of it: each character gets the
    /// damage times a random factor from 1 - spread to 1 + spread. From 0 to 1.
    float spread = 0.3f;
};

/// The hit points of an ordinary person, which a landing at FallDamageConfig::full_damage_speed takes on average.
inline constexpr float kFullFallDamage = 100.0f;

/// The damage of a landing at `impact_speed` before the spread, see FallDamageConfig: 0 up to `safe_speed`.
///
/// \param impact_speed The vertical speed of the landing, in metres per second, positive downwards.
float base_fall_damage(const FallDamageConfig& config, float impact_speed);

/// The damage that one character takes from a landing at `impact_speed`: base_fall_damage() times a random factor from
/// 1 - FallDamageConfig::spread to 1 + spread, rounded to whole hit points. Draws a random number only if
/// base_fall_damage() is positive.
int fall_damage(const FallDamageConfig& config, float impact_speed, Rng& rng);

/// Hurts each character of `party` by its own fall_damage() of a landing at `impact_speed`, independently of the
/// others, down to 0 hit points. Dead characters, see Character::dead(), take no damage and draw no random numbers.
///
/// \return The most damage that a character took, after the clamping to 0 hit points: 0 if none was hurt.
int apply_fall_damage(std::span<Character> party, const FallDamageConfig& config, float impact_speed, Rng& rng);

}  // namespace manifold
