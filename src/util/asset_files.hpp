#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace manifold {

/// Name of the directory of the game's textures and sounds, next to the executable in a copy of the game.
inline constexpr std::string_view kAssetsDirName = "assets";

/// Finds the directory of the game's textures and sounds: kAssetsDirName next to the executable, if there is one,
/// so that a copy of the game with its assets runs anywhere, otherwise `fallback`.
///
/// \param executable_dir The directory of the executable, or an empty path if it isn't known.
/// \param fallback       Where the assets are otherwise, e.g. in the source tree.
std::filesystem::path find_assets_dir(const std::filesystem::path& executable_dir,
                                      const std::filesystem::path& fallback);

/// Finds all files with the extension `extension` (such as ".png") under `root`: directly in it and in all its
/// subdirectories, at any depth.
///
/// Directories that can't be read are skipped. If the search stops early because of another error, a warning is
/// logged. The extension is matched without regard to case, e.g. `.PNG` is found for ".png".
///
/// \param extension With the leading dot, in any case.
/// \return The paths, sorted, so that the same files always give the same order. Empty if `root` doesn't exist.
std::vector<std::filesystem::path> find_asset_files(const std::filesystem::path& root, std::string_view extension);

/// Picks one of `candidates` at random. The choice depends only on `seed` and the candidates.
///
/// \return The picked path, or an empty optional if there are no candidates.
std::optional<std::filesystem::path> pick_asset_file(std::span<const std::filesystem::path> candidates,
                                                     std::uint64_t seed);

}  // namespace manifold
