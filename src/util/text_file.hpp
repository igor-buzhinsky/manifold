#pragma once

#include <filesystem>
#include <string>

namespace manifold {

/// Reads a whole file, such as a save or a settings file, as it is: bytes, without converting line ends.
///
/// \throws std::runtime_error, with the path in its message, if `path` is not a regular file (e.g. it doesn't exist
///         or is a directory) or can't be opened or read.
std::string read_text_file(const std::filesystem::path& path);

}  // namespace manifold
