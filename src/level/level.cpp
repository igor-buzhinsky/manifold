#include "level/level.hpp"

#include <array>
#include <cstddef>
#include <random>

#include "world/torch.hpp"

namespace manifold {

TorchFlame choose_torch_flame(LevelRng& rng) {
    constexpr std::array<TorchFlame, 3> kFlames{TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen};
    std::discrete_distribution<std::size_t> distribution({0.5, 0.25, 0.25});
    return kFlames.at(distribution(rng));
}

const char* texture_kind_name(TextureKind kind) {
    switch (kind) {
        case TextureKind::kGrass:
            return "grass";
        case TextureKind::kWood:
            return "wood";
        case TextureKind::kMetal:
            return "metal";
        case TextureKind::kStone:
            return "stone";
    }
    return "unknown";
}

}  // namespace manifold
