#include "audio/sound.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>

// Only the declarations: the implementation is built from stb_vorbis.c as a C library, see CMakeLists.txt.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>  // NOLINT(bugprone-suspicious-include)

#include "util/log.hpp"

namespace manifold {

float Sound::duration() const {
    if (channels <= 0 || sample_rate <= 0) {
        return 0.0f;
    }
    return static_cast<float>(samples.size()) / static_cast<float>(channels) / static_cast<float>(sample_rate);
}

std::optional<Sound> load_sound(const std::filesystem::path& path) {
    int channels = 0;
    int sample_rate = 0;
    short* data = nullptr;
    // The number of samples per channel, or a negative number if the file can't be read or decoded.
    const int frames = stb_vorbis_decode_filename(path.string().c_str(), &channels, &sample_rate, &data);
    if (frames <= 0 || channels <= 0 || sample_rate <= 0 || data == nullptr) {
        log_warning("Can't load {}: not a readable OGG Vorbis file with sound in it", path.string());
        // stb_vorbis allocates the samples with malloc().
        std::free(data);  // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        return std::nullopt;
    }
    Sound sound{.channels = channels, .sample_rate = sample_rate, .samples = {}};
    const auto size = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
    sound.samples.assign(data, data + size);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::free(data);                          // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    return sound;
}

}  // namespace manifold
