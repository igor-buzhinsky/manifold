#include "level/level.hpp"

#include <array>

#include "util/random.hpp"
#include "world/torch.hpp"

namespace manifold {

TorchFlame choose_torch_flame(LevelRng& rng) {
    constexpr std::array<TorchFlame, 3> kFlames{TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen};
    constexpr std::array<double, 3> kWeights{0.5, 0.25, 0.25};
    return kFlames.at(random_weighted_index(rng, kWeights));
}

}  // namespace manifold
