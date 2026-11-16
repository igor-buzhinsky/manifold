#include "audio/sound_set.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

#include "audio/sound.hpp"
#include "util/asset_files.hpp"
#include "util/log.hpp"

namespace manifold {

SoundSet::SoundSet(const std::filesystem::path& root, const char* name) {
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

}  // namespace manifold
