#include "render/texture_image.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <stb_image.h>

#include "util/log.hpp"
#include "world/door.hpp"

namespace manifold {

namespace {

constexpr int kChannels = 4;

/// Returns true if `path` ends with `.png`, in any case.
bool has_png_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".png";
}

}  // namespace

std::optional<Image> load_image(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int file_channels = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &file_channels, kChannels);
    if (data == nullptr) {
        log_warning("Can't load {}: {}", path.string(), stbi_failure_reason());
        return std::nullopt;
    }
    Image image{.width = width, .height = height, .pixels = {}};
    const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels;
    image.pixels.assign(data, data + size);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    stbi_image_free(data);
    return image;
}

std::vector<std::filesystem::path> find_textures(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied,
                                                     error);
    for (const std::filesystem::recursive_directory_iterator end; !error && it != end; it.increment(error)) {
        if (it->is_regular_file(error) && has_png_extension(it->path())) {
            paths.push_back(it->path());
        }
    }
    // A missing root is normal (no textures), but other errors end the search early.
    if (error && error != std::errc::no_such_file_or_directory) {
        log_warning("Searching for textures in {} stopped early: {}", root.string(), error.message());
    }
    std::ranges::sort(paths);
    return paths;
}

std::optional<PickedTexture> pick_texture(std::span<const std::filesystem::path> candidates, std::uint64_t seed) {
    std::vector<std::filesystem::path> order(candidates.begin(), candidates.end());
    std::mt19937_64 random(seed);
    std::ranges::shuffle(order, random);
    for (const std::filesystem::path& path : order) {
        // Reading only the header first avoids decoding a large image just to skip it. Files whose header
        // can't be read are left to load_image(), which logs why.
        int width = 0;
        int height = 0;
        int channels = 0;
        if (stbi_info(path.string().c_str(), &width, &height, &channels) != 0 &&
            (width > kMaxTextureSize || height > kMaxTextureSize)) {
            log_warning("Texture {} is {}x{}, larger than {}x{}, trying another one", path.string(), width, height,
                        kMaxTextureSize, kMaxTextureSize);
            continue;
        }
        std::optional<Image> image = load_image(path);
        if (!image) {
            continue;
        }
        log_info("Texture: {} ({}x{})", path.string(), image->width, image->height);
        return PickedTexture{.path = path, .image = std::move(*image)};
    }
    return std::nullopt;
}

glm::vec3 mean_linear_color(const Image& image) {
    // The linear value of each of the 256 sRGB byte values.
    static const std::array<float, 256> linear = [] {
        std::array<float, 256> table{};
        for (std::size_t i = 0; i < table.size(); ++i) {
            const float srgb = static_cast<float>(i) / 255.0f;
            table[i] = srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        }
        return table;
    }();
    constexpr auto kStride = static_cast<std::size_t>(kChannels);
    const std::size_t pixel_count = image.pixels.size() / kStride;
    if (pixel_count == 0) {
        return glm::vec3(0.0f);
    }
    // Sums in double keep the rounding errors small for large images.
    std::array<double, 3> sum{};
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        for (std::size_t channel = 0; channel < sum.size(); ++channel) {
            sum.at(channel) += linear.at(image.pixels[(pixel * kStride) + channel]);
        }
    }
    const auto count = static_cast<double>(pixel_count);
    return {static_cast<float>(sum[0] / count), static_cast<float>(sum[1] / count), static_cast<float>(sum[2] / count)};
}

Image fallback_texture() { return {.width = 1, .height = 1, .pixels = {128, 128, 128, 255}}; }

namespace {

/// A pseudo-random number in [0, 1) for a pixel, for the grain of wood and metal.
float pixel_noise(int x, int y) {
    auto hash = static_cast<std::uint32_t>((x * 73856093) ^ (y * 19349663));
    hash ^= hash >> 13U;
    hash *= 0x5bd1e995U;
    hash ^= hash >> 15U;
    return static_cast<float>(hash & 0xffffU) / 65536.0f;
}

/// An sRGB color of a door texture, in [0, 255] before it is written as bytes.
struct DoorColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/// A grey with a slight blue tint, for steel and iron, `brightness` times a base grey.
DoorColor steel(float brightness) {
    return {.r = 100.0f * brightness, .g = 103.0f * brightness, .b = 110.0f * brightness};
}

/// The color of a pixel of the wooden door texture, see make_door_texture().
DoorColor wood_pixel(int x, int y) {
    constexpr int kPlankWidth = kDoorTextureSize / 4;
    constexpr int kBandTop = 38;     // First row of the iron band.
    constexpr int kBandBottom = 49;  // Last row of the iron band.
    constexpr int kRivetTop = 42;    // First of the two rows of the rivets.
    if (y >= kBandTop && y <= kBandBottom) {
        const int plank_x = x % kPlankWidth;
        const bool rivet = (plank_x == kPlankWidth / 2 || plank_x == (kPlankWidth / 2) - 1) &&
                           (y == kRivetTop || y == kRivetTop + 1 || y == kRivetTop + 3 || y == kRivetTop + 4);
        if (rivet) {
            return {.r = 150.0f, .g = 148.0f, .b = 140.0f};
        }
        // A lighter top edge and a darker bottom edge make the band stand out from the planks.
        float shade = 1.0f;
        if (y == kBandTop) {
            shade = 1.4f;
        } else if (y == kBandBottom) {
            shade = 0.6f;
        }
        const float rust = 8.0f * pixel_noise(x, y);
        return {.r = (66.0f + rust) * shade, .g = (64.0f + (rust / 2.0f)) * shade, .b = 62.0f * shade};
    }
    if (x % kPlankWidth == 0) {
        return {.r = 30.0f, .g = 19.0f, .b = 10.0f};  // The gap between two planks.
    }
    // Each plank has its own tint. The grain runs along the planks: it changes slowly upwards and quickly across.
    const int plank = x / kPlankWidth;
    const float tint = 0.85f + (0.3f * pixel_noise(plank, 7));
    const float grain = (0.8f + (0.4f * pixel_noise(x, y / 6))) * (0.95f + (0.1f * pixel_noise(x, y)));
    const float light = tint * grain;
    return {.r = 112.0f * light, .g = 74.0f * light, .b = 42.0f * light};
}

/// The color of a pixel of the metal door texture, see make_door_texture().
DoorColor metal_pixel(int x, int y) {
    constexpr int kPlateWidth = kDoorTextureSize / 2;
    constexpr int kRibTop = 40;     // First row of the rib.
    constexpr int kRibBottom = 47;  // Last row of the rib.
    constexpr int kRivetSpacing = 8;
    if (y >= kRibTop && y <= kRibBottom) {
        // A rounded rib: lit from above, in shadow below, with a rivet every kRivetSpacing pixels. A rivet is a small
        // dome of 3 x 3 pixels without corners, lit on its top and left, in shadow on its bottom and right.
        const int rivet_x = (x % kRivetSpacing) - 4;
        const int rivet_y = y - (kRibTop + 3);
        if (std::abs(rivet_x) + std::abs(rivet_y) <= 1) {
            if (rivet_y < 0 || rivet_x < 0) {
                return steel(2.2f);
            }
            return steel(rivet_y > 0 || rivet_x > 0 ? 0.6f : 1.6f);
        }
        constexpr std::array<float, kRibBottom - kRibTop + 1> kRibShade{1.8f, 1.5f, 1.3f, 1.2f,
                                                                        1.1f, 1.0f, 0.8f, 0.45f};
        return steel(kRibShade.at(static_cast<std::size_t>(y - kRibTop)) * (0.97f + (0.06f * pixel_noise(x, y))));
    }
    // A seam between two plates: a dark gap with a lit edge on its right.
    const int plate_x = x % kPlateWidth;
    if (plate_x == 0) {
        return steel(0.35f);
    }
    if (plate_x == 1) {
        return steel(1.5f);
    }
    // Brushed steel: fine streaks that run up the plate, and a slight difference between the plates.
    const float tint = 0.95f + (0.1f * pixel_noise(x / kPlateWidth, 3));
    const float streak = (0.92f + (0.16f * pixel_noise(x, 5))) * (0.97f + (0.06f * pixel_noise(x, y)));
    return steel(1.15f * tint * streak);
}

}  // namespace

Image make_door_texture(DoorLook look) {
    Image image{.width = kDoorTextureSize, .height = kDoorTextureSize, .pixels = {}};
    image.pixels.reserve(static_cast<std::size_t>(kDoorTextureSize) * kDoorTextureSize * kChannels);
    const auto to_byte = [](float value) { return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f)); };
    for (int y = 0; y < kDoorTextureSize; ++y) {
        for (int x = 0; x < kDoorTextureSize; ++x) {
            const DoorColor color = look == DoorLook::kMetal ? metal_pixel(x, y) : wood_pixel(x, y);
            image.pixels.insert(image.pixels.end(),
                                {to_byte(color.r), to_byte(color.g), to_byte(color.b), std::uint8_t{255}});
        }
    }
    return image;
}

}  // namespace manifold
