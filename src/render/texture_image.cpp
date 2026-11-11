#include "render/texture_image.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <stb_image.h>

#include "util/log.hpp"

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
    for (std::filesystem::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        if (it->is_regular_file(error) && has_png_extension(it->path())) {
            paths.push_back(it->path());
        }
    }
    std::ranges::sort(paths);
    return paths;
}

std::optional<PickedTexture> pick_texture(std::span<const std::filesystem::path> candidates, std::uint64_t seed) {
    std::vector<std::filesystem::path> order(candidates.begin(), candidates.end());
    std::mt19937_64 random(seed);
    std::ranges::shuffle(order, random);
    for (const std::filesystem::path& path : order) {
        std::optional<Image> image = load_image(path);
        if (!image) {
            continue;
        }
        if (image->width > kMaxTextureSize || image->height > kMaxTextureSize) {
            log_warning("Texture {} is {}x{}, larger than {}x{}, trying another one", path.string(), image->width,
                        image->height, kMaxTextureSize, kMaxTextureSize);
            continue;
        }
        log_info("Texture: {} ({}x{})", path.string(), image->width, image->height);
        return PickedTexture{.path = path, .image = std::move(*image)};
    }
    return std::nullopt;
}

Image fallback_texture() { return {.width = 1, .height = 1, .pixels = {128, 128, 128, 255}}; }

}  // namespace manifold
