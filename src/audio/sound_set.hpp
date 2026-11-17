#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
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
    SoundSet(const std::filesystem::path& root, std::string_view name);

    std::size_t size() const { return sounds_.size(); }

    /// The sound with the number `number`, counting round: sound `number` modulo size(), e.g. for DoorSounds.
    ///
    /// \return The sound, or null if the set is empty. It lives as long as the set.
    const Sound* get(std::uint32_t number) const;

    /// A random sound number for get(), other than `previous` when the set has more than one sound, e.g. so that two
    /// footsteps in a row don't sound the same.
    ///
    /// \param previous The number of the sound before, or any number if there was none.
    /// \param random   A random number, which picks among the other sounds.
    /// \return A number in [0, size()), or 0 for an empty set.
    std::uint32_t pick_other(std::uint32_t previous, std::uint64_t random) const;

private:
    std::vector<Sound> sounds_;
};

}  // namespace manifold
