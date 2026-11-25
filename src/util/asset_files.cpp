#include "util/asset_files.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "util/log.hpp"

namespace manifold {

namespace {

/// `text` in lowercase.
std::string to_lower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

/// Returns true if the extension of `path` is `extension`, without regard to case.
bool has_extension(const std::filesystem::path& path, std::string_view extension) {
    return to_lower(path.extension().string()) == to_lower(extension);
}

}  // namespace

std::filesystem::path find_assets_dir(const std::filesystem::path& executable_dir,
                                      const std::filesystem::path& fallback) {
    if (!executable_dir.empty()) {
        std::filesystem::path next_to_executable = executable_dir / kAssetsDirName;
        std::error_code error;
        if (std::filesystem::is_directory(next_to_executable, error)) {
            return next_to_executable;
        }
    }
    return fallback;
}

std::vector<std::filesystem::path> find_asset_files(const std::filesystem::path& root, std::string_view extension) {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied,
                                                     error);
    for (const std::filesystem::recursive_directory_iterator end; !error && it != end; it.increment(error)) {
        if (it->is_regular_file(error) && has_extension(it->path(), extension)) {
            paths.push_back(it->path());
        }
    }
    // A missing root is normal (no assets), but other errors end the search early.
    if (error && error != std::errc::no_such_file_or_directory) {
        log_warning("Searching for {} files in {} stopped early: {}", extension, root.string(), error.message());
    }
    std::ranges::sort(paths);
    return paths;
}

}  // namespace manifold
