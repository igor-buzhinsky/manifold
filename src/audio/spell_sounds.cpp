#include "audio/spell_sounds.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>

#include "audio/sound.hpp"
#include "party/spells.hpp"
#include "util/asset_mapping.hpp"
#include "util/log.hpp"

namespace manifold {

SpellSounds::SpellSounds(const AssetMapping& mapping) {
    std::size_t loaded = 0;
    for (const Spell spell : kAllSpells) {
        const std::optional<std::filesystem::path> path = mapping.spell_sound(spell_key(spell));
        if (!path) {
            log_warning("Spell {} has no sound in {}", spell_key(spell), kSpellSoundsKey);
            continue;
        }
        std::optional<Sound>& sound = sounds_.at(static_cast<std::size_t>(spell));
        sound = load_sound(*path);
        if (sound) {
            ++loaded;
        }
    }
    for (const auto& [key, path] : mapping.spell_sounds()) {
        if (!parse_spell_key(key)) {
            log_warning("{} has a sound for {}, which is no spell", kSpellSoundsKey, key);
        }
    }
    log_info("{} of {} {} loaded", loaded, kAllSpells.size(), kSpellSoundsKey);
}

const Sound* SpellSounds::get(Spell spell) const {
    const std::optional<Sound>& sound = sounds_.at(static_cast<std::size_t>(spell));
    return sound ? &*sound : nullptr;
}

}  // namespace manifold
