#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"

namespace manifold {

/// Reads lines typed into the terminal that the game was started from, or piped into it, without waiting.
///
/// Uses POSIX functions on the standard input. While the game runs in the background of a shell, nothing is
/// read.
class TerminalInput {
public:
    /// The complete lines that arrived since the last call, without line breaks. Never waits for input.
    ///
    /// After the end of the input, e.g. when it is not a terminal and is empty, returns nothing.
    std::vector<std::string> read_lines();

private:
    /// Marks the end of the input. The last line counts as complete.
    void end();

    LineBuffer buffer_;
    bool ended_ = false;  ///< Whether the end of the input was reached.
};

}  // namespace manifold
