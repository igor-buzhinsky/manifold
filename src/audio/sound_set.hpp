#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "audio/sound.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {

/// The sounds of one key of the asset mapping, e.g. of doors that start to open, loaded, see AssetMapping.
class SoundSet {
public:
    /// An empty set.
    SoundSet() = default;

    /// Loads each of `paths`, in order: sound i is loaded from `paths[i]`.
    ///
    /// A file that can't be loaded keeps its place but has no sound, after load_sound() logged a warning. How many
    /// sounds were loaded is logged as info, with `name` (e.g. "DOOR_OPEN_SOUNDS"). No paths give an empty set.
    SoundSet(std::span<const std::filesystem::path> paths, std::string_view name);

    /// Loads the files of `key`, see the other constructor, with the key's name.
    SoundSet(const AssetMapping& mapping, AssetKey key);

    /// How many sounds the set has, also those whose files couldn't be loaded.
    std::size_t size() const { return sounds_.size(); }

    /// Sound `index`.
    ///
    /// \return The sound, or null if its file couldn't be loaded, or if there is no such sound (after logging a
    ///         warning, unless the set is empty). It lives as long as the set.
    const Sound* get(AssetIndex index) const;

    /// A random sound index for get(), other than `previous` when the set has more than one sound, e.g. so that two
    /// footsteps in a row don't sound the same.
    ///
    /// \param previous The index of the sound before, or any number if there was none.
    /// \param random   A random number, which picks among the other sounds.
    /// \return An index in [0, size()), or 0 for an empty set.
    AssetIndex pick_other(AssetIndex previous, std::uint64_t random) const;

private:
    std::string name_;  ///< For warnings, see the constructor.
    std::vector<std::optional<Sound>> sounds_;
};

}  // namespace manifold
