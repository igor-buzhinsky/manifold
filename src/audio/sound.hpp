#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace manifold {

/// A sound in memory, ready to play: signed 16-bit samples, interleaved by channel.
struct Sound {
    int channels = 0;     ///< 1 for mono, 2 for stereo, and so on.
    int sample_rate = 0;  ///< Samples per second, per channel.
    /// `channels` samples per moment, one for each channel, moment by moment from the start.
    std::vector<std::int16_t> samples;

    /// How long the sound plays, in seconds. 0 for an empty sound.
    float duration() const;
};

/// Loads and decodes an OGG Vorbis file.
///
/// \return The sound, or an empty optional (after logging a warning) if the file can't be read or decoded, or has no
///         samples.
std::optional<Sound> load_sound(const std::filesystem::path& path);

}  // namespace manifold
