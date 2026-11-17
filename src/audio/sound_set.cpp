#include "audio/sound_set.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

#include "audio/sound.hpp"
#include "util/asset_files.hpp"
#include "util/log.hpp"

namespace manifold {

SoundSet::SoundSet(const std::filesystem::path& root, std::string_view name) {
    for (const std::filesystem::path& path : find_asset_files(root, ".ogg")) {
        std::optional<Sound> sound = load_sound(path);
        if (sound) {
            sounds_.push_back(std::move(*sound));
        }
    }
    if (sounds_.empty()) {
        log_warning("No {} sounds found in {}", name, root.string());
    } else {
        log_info("{} {} sounds from {}", sounds_.size(), name, root.string());
    }
}

const Sound* SoundSet::get(std::uint32_t number) const {
    if (sounds_.empty()) {
        return nullptr;
    }
    return &sounds_[number % sounds_.size()];
}

std::uint32_t SoundSet::pick_other(std::uint32_t previous, std::uint64_t random) const {
    const std::size_t count = sounds_.size();
    if (count <= 1) {
        return 0;
    }
    // One of the count - 1 others: counting on from `previous`, skipping it.
    const std::size_t skip = 1 + (random % (count - 1));
    return static_cast<std::uint32_t>(((previous % count) + skip) % count);
}

}  // namespace manifold
