#pragma once

#include <array>
#include <cstdint>

namespace manifold {

/// What the surfaces of a level are made of, as its texture shows, e.g. to choose the sound of footsteps.
///
/// Each kind has a key of textures and one of footstep sounds in the asset mapping, see level_textures_key() and
/// footstep_sounds_key(), and its files are in directories named by texture_kind_name(), e.g. `assets/textures/grass/`
/// and `assets/sounds/footsteps/grass/`.
enum class TextureKind : std::uint8_t {
    kGrass,  ///< Grass.
    kWood,   ///< Wooden planks and boards.
    kMetal,  ///< Metal plates.
    kStone,  ///< Everything else that is hard underfoot: bricks, tiles, stone, plaster and dirt.
};

/// All texture kinds, in the order of TextureKind.
inline constexpr std::array<TextureKind, 4> kTextureKinds{TextureKind::kGrass, TextureKind::kWood, TextureKind::kMetal,
                                                          TextureKind::kStone};

/// The name of a texture kind, which is also the name of its directories: `grass`, `wood`, `metal` or `stone`.
const char* texture_kind_name(TextureKind kind);

}  // namespace manifold
