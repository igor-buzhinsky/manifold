#include "level/level.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "level/texture_kind.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"
#include "world/torch.hpp"

namespace manifold {

TorchFlame choose_torch_flame(LevelRng& rng, const LevelObjectChances& chances) {
    constexpr std::array<TorchFlame, 3> kFlames{TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen};
    return kFlames.at(random_weighted_index(rng, chances.torch_flames));
}

ChestLook choose_chest_look(LevelRng& rng, const LevelObjectChances& chances) {
    return random_chance(rng, chances.golden_chest) ? ChestLook::kGold : ChestLook::kWood;
}

bool level_has_ambient_light(std::uint64_t level_seed) {
    // Mixing the seed keeps the choice independent of the other random choices made from it.
    constexpr std::uint64_t kAmbientSeedSalt = 0xd1b54a32d192ed03ULL;
    Rng random(level_seed ^ kAmbientSeedSalt);
    return random_chance(random, 0.5);
}

LevelTexture choose_level_texture(const AssetCounts& counts, std::uint64_t level_seed) {
    // Mixing the seed keeps the choice independent of the random numbers used by level generation.
    constexpr std::uint64_t kTextureSeedSalt = 0x9e3779b97f4a7c15ULL;
    std::size_t total = 0;
    for (const TextureKind kind : kTextureKinds) {
        total += counts.of(level_textures_key(kind));
    }
    if (total == 0) {
        return {};
    }
    Rng random(level_seed ^ kTextureSeedSalt);
    // The files of all kinds, one kind after another in the order of kTextureKinds.
    std::size_t chosen = random_index(random, total);
    for (const TextureKind kind : kTextureKinds) {
        const std::size_t count = counts.of(level_textures_key(kind));
        if (chosen < count) {
            return {.kind = kind, .index = static_cast<AssetIndex>(chosen)};
        }
        chosen -= count;
    }
    return {};
}

}  // namespace manifold
