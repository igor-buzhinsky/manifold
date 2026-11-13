#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"

namespace manifold {

/// Reads lines typed into the terminal that the game was started from, or piped into it, without waiting.
///
/// Uses POSIX functions on the standard input.
class TerminalInput {
public:
    /// The complete lines that arrived since the last call, without line breaks. Never waits for input.
    ///
    /// After the end of the input, e.g. when it is not a terminal and is empty, returns nothing.
    std::vector<std::string> read_lines();

private:
    LineBuffer buffer_;
    bool ended_ = false;  ///< Whether the end of the input was reached.
};

}  // namespace manifold
