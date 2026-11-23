#include "util/text_file.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

namespace manifold {

std::string read_text_file(const std::filesystem::path& path) {
    // A directory can be opened as a stream on some platforms, and then reads as empty.
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        throw std::runtime_error("not a file: " + path.string());
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("can't open " + path.string());
    }
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        throw std::runtime_error("can't read " + path.string());
    }
    return text;
}

}  // namespace manifold
