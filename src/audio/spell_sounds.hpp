#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "audio/sound.hpp"
#include "party/spells.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {

/// The casting sound of each spell, loaded from `SPELL_SOUNDS` of the asset mapping, see AssetMapping::spell_sound().
class SpellSounds {
public:
    /// No sounds.
    SpellSounds() = default;

    /// Loads the sound of each spell. A spell without a sound in the mapping, or whose file can't be loaded, has none,
    /// after a warning. A key of `SPELL_SOUNDS` that is no spell is ignored, after a warning. How many sounds were
    /// loaded is logged as info.
    explicit SpellSounds(const AssetMapping& mapping);

    /// The sound of casting `spell`, or null if it has none. It lives as long as the object.
    const Sound* get(Spell spell) const;

private:
    std::array<std::optional<Sound>, kAllSpells.size()> sounds_;  ///< In the order of kAllSpells.
};

}  // namespace manifold
