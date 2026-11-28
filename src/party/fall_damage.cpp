#include "party/fall_damage.hpp"

#include <algorithm>
#include <cmath>
#include <span>

#include "party/party.hpp"
#include "util/random.hpp"

namespace manifold {

float base_fall_damage(const FallDamageConfig& config, float impact_speed) {
    if (impact_speed <= config.safe_speed) {
        return 0.0f;
    }
    const float safe = config.safe_speed * config.safe_speed;
    const float full = config.full_damage_speed * config.full_damage_speed;
    return kFullFallDamage * ((impact_speed * impact_speed) - safe) / (full - safe);
}

int fall_damage(const FallDamageConfig& config, float impact_speed, Rng& rng) {
    const float damage = base_fall_damage(config, impact_speed);
    if (damage <= 0.0f) {
        return 0;
    }
    const float factor = random_float(rng, 1.0f - config.spread, 1.0f + config.spread);
    return static_cast<int>(std::lround(damage * factor));
}

int apply_fall_damage(std::span<Character> party, const FallDamageConfig& config, float impact_speed, Rng& rng) {
    int most = 0;
    for (Character& character : party) {
        if (character.dead()) {
            continue;
        }
        const int damage = std::min(fall_damage(config, impact_speed, rng), character.hp);
        character.hp -= damage;
        most = std::max(most, damage);
    }
    return most;
}

}  // namespace manifold
