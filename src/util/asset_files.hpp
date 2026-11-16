#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace manifold {

/// Finds all files with the extension `extension` (such as ".png") under `root`: directly in it and in all its
/// subdirectories, at any depth.
///
/// Directories that can't be read are skipped. If the search stops early because of another error, a warning is
/// logged. The extension is matched without regard to case, e.g. `.PNG` is found for ".png".
///
/// \param extension Lowercase, with the leading dot.
/// \return The paths, sorted, so that the same files always give the same order. Empty if `root` doesn't exist.
std::vector<std::filesystem::path> find_asset_files(const std::filesystem::path& root, std::string_view extension);

/// Picks one of `candidates` at random. The choice depends only on `seed` and the candidates.
///
/// \return The picked path, or an empty optional if there are no candidates.
std::optional<std::filesystem::path> pick_asset_file(std::span<const std::filesystem::path> candidates,
                                                     std::uint64_t seed);

}  // namespace manifold
