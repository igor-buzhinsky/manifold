#include "level/level.hpp"

#include <array>
#include <cstdint>

#include "util/random.hpp"
#include "world/torch.hpp"

namespace manifold {

TorchFlame choose_torch_flame(LevelRng& rng) {
    constexpr std::array<TorchFlame, 3> kFlames{TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen};
    constexpr std::array<double, 3> kWeights{0.5, 0.25, 0.25};
    return kFlames.at(random_weighted_index(rng, kWeights));
}

bool level_has_ambient_light(std::uint64_t level_seed) {
    // Mixing the seed keeps the choice independent of the other random choices made from it.
    constexpr std::uint64_t kAmbientSeedSalt = 0xd1b54a32d192ed03ULL;
    Rng random(level_seed ^ kAmbientSeedSalt);
    return random_chance(random, 0.5);
}

}  // namespace manifold
