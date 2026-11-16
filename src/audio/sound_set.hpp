#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "audio/sound.hpp"

namespace manifold {

/// The sounds of one kind, e.g. of doors that start to open: all OGG files under a directory, loaded.
class SoundSet {
public:
    /// An empty set.
    SoundSet() = default;

    /// Loads all OGG files under `root`, at any depth, in the order of find_asset_files().
    ///
    /// Files that can't be loaded are left out, after load_sound() logged a warning. How many sounds were loaded is
    /// logged as info, with `name` (e.g. "door open"). A missing `root` gives an empty set, with a warning.
    SoundSet(const std::filesystem::path& root, const char* name);

    std::size_t size() const { return sounds_.size(); }

    /// The sound with the number `number`, counting round: sound `number` modulo size(), e.g. for DoorSounds.
    ///
    /// \return The sound, or null if the set is empty. It lives as long as the set.
    const Sound* get(std::uint32_t number) const;

private:
    std::vector<Sound> sounds_;
};

}  // namespace manifold
