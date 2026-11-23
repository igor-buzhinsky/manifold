#pragma once

#include <filesystem>
#include <span>
#include <string_view>

namespace manifold {

/// Name of a settings file: of the game's own, next to the executable or in the source tree, and of the player's, in
/// the game's directory for user data.
inline constexpr std::string_view kSettingsFileName = "settings.json";

/// Settings of the game that the player can change, read from settings files, see load_settings().
struct Settings {
    /// Whether the game fills the screen at the screen's own resolution, with black bars where the screen's aspect
    /// ratio differs from the picture's. Otherwise it runs in a window.
    bool fullscreen = false;

    bool operator==(const Settings&) const = default;
};

/// Reads the settings of a JSON object over `base`: the fields that `json` has replace those of `base`, the others
/// stay.
///
/// The fields are `fullscreen` (a boolean). Unknown fields are ignored with a warning, so a misspelled one is noticed.
///
/// \throws std::runtime_error if `json` is not a valid JSON object or a field has a wrong type.
Settings parse_settings(std::string_view json, const Settings& base);

/// Reads settings files in order, each over the ones before it, see parse_settings(), so the last one has priority.
///
/// A file that doesn't exist is skipped. A file that can't be read or parsed, or a directory, is skipped as a whole
/// with an error in the log, so the game still starts.
///
/// \return The default Settings with the files read over them.
Settings load_settings(std::span<const std::filesystem::path> files);

/// Finds the game's own settings file: kSettingsFileName next to the executable, if there is one, as in a copy of the
/// game, otherwise `fallback`.
///
/// \param executable_dir The directory of the executable, or an empty path if it isn't known.
/// \param fallback       Where the file is otherwise, e.g. in the source tree.
std::filesystem::path find_game_settings_file(const std::filesystem::path& executable_dir,
                                              const std::filesystem::path& fallback);

}  // namespace manifold
