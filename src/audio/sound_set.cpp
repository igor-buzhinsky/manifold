#include "audio/sound_set.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

#include "audio/sound.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/log.hpp"

namespace manifold {

SoundSet::SoundSet(std::span<const std::filesystem::path> paths, std::string_view name) : name_(name) {
    std::size_t loaded = 0;
    sounds_.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        sounds_.push_back(load_sound(path));
        if (sounds_.back()) {
            ++loaded;
        }
    }
    log_info("{} of {} {} loaded", loaded, sounds_.size(), name);
}

SoundSet::SoundSet(const AssetMapping& mapping, AssetKey key) : SoundSet(mapping.paths(key), asset_key_name(key)) {}

const Sound* SoundSet::get(AssetIndex index) const {
    if (index >= sounds_.size()) {
        if (!sounds_.empty()) {
            log_warning("{} has no sound {}: it has {}", name_, index, sounds_.size());
        }
        return nullptr;
    }
    const std::optional<Sound>& sound = sounds_[index];
    return sound ? &*sound : nullptr;
}

AssetIndex SoundSet::pick_other(AssetIndex previous, std::uint64_t random) const {
    const std::size_t count = sounds_.size();
    if (count <= 1) {
        return 0;
    }
    // One of the count - 1 others: counting on from `previous`, skipping it.
    const std::size_t skip = 1 + (random % (count - 1));
    return static_cast<AssetIndex>(((previous % count) + skip) % count);
}

}  // namespace manifold
