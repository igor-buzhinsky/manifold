#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace manifold {

/// Textures larger than this in either dimension, in pixels, are not used.
constexpr int kMaxTextureSize = 128;

/// An image in memory, 8-bit RGBA, with sRGB colors.
struct Image {
    int width = 0;   ///< In pixels.
    int height = 0;  ///< In pixels.
    /// Four bytes (R, G, B, A) per pixel, row by row from the top row of the image.
    std::vector<std::uint8_t> pixels;
};

/// A texture picked for a level, see pick_texture().
struct PickedTexture {
    std::filesystem::path path;  ///< The file it was loaded from.
    Image image;
};

/// Loads a PNG file and converts it to RGBA.
///
/// \return The image, or an empty optional (after logging a warning) if the file can't be read or decoded.
std::optional<Image> load_image(const std::filesystem::path& path);

/// Finds all PNG files under `root`: directly in it and in all its subdirectories.
///
/// Directories that can't be read are skipped. If the search stops early because of another error, a
/// warning is logged. The extension is matched without regard to case, e.g. `.PNG` is found too.
///
/// \return The paths, sorted, so that the same files always give the same order. Empty if `root` doesn't
///         exist.
std::vector<std::filesystem::path> find_textures(const std::filesystem::path& root);

/// Picks a random texture among `candidates` and loads it.
///
/// Candidates are tried in a random order that depends only on `seed`. A file that can't be loaded, or that
/// is larger than kMaxTextureSize in either dimension, is skipped with a warning, and the next one is tried.
/// The texture that is used is logged as info.
///
/// \return The first texture that loads and is small enough, or an empty optional if there is none.
std::optional<PickedTexture> pick_texture(std::span<const std::filesystem::path> candidates, std::uint64_t seed);

/// A plain grey 1 x 1 image, used when no texture can be loaded.
Image fallback_texture();

/// Width and height of the image of make_gate_texture(), in pixels.
inline constexpr int kGateTextureSize = 64;

/// Makes the texture of door halves: an opaque gate of four upright wooden planks with an iron band across them.
///
/// The image is kGateTextureSize pixels square and repeats seamlessly upwards. It is made in code, so it doesn't
/// depend on an asset file, and is the same every time.
Image make_gate_texture();

}  // namespace manifold
