#include "util/asset_files.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "util/log.hpp"

namespace manifold {

namespace {

/// Returns true if the extension of `path`, in lowercase, is `extension`.
bool has_extension(const std::filesystem::path& path, std::string_view extension) {
    std::string actual = path.extension().string();
    std::ranges::transform(actual, actual.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return actual == extension;
}

}  // namespace

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

std::optional<std::filesystem::path> pick_asset_file(std::span<const std::filesystem::path> candidates,
                                                     std::uint64_t seed) {
    if (candidates.empty()) {
        return std::nullopt;
    }
    // The raw output of the engine is the same everywhere, unlike std::uniform_int_distribution. The slight bias of
    // the modulo doesn't matter for a handful of files.
    std::mt19937_64 random(seed);
    return candidates[random() % candidates.size()];
}

}  // namespace manifold
