#include "platform/terminal_input.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/log.hpp"

namespace manifold {

void TerminalInput::end() {
    ended_ = true;
    buffer_.append("\n");  // The last line may have no line break.
}

std::vector<std::string> TerminalInput::read_lines() {
    // A game started in the background of a shell (`manifold &`) would be stopped by reading the terminal,
    // so it reads only while it is in the foreground.
    if (isatty(STDIN_FILENO) != 0 && tcgetpgrp(STDIN_FILENO) != getpgrp()) {
        return {};
    }
    std::array<char, 4096> chunk{};
    while (!ended_) {
        pollfd input{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        if (poll(&input, 1, 0) <= 0) {
            break;  // Nothing to read now.
        }
        if ((input.revents & (POLLIN | POLLHUP)) == 0) {
            end();  // E.g. there is no standard input.
            break;
        }
        const ssize_t count = read(STDIN_FILENO, chunk.data(), chunk.size());
        if (count <= 0) {
            end();
            if (count < 0) {
                log_warning("Can't read console commands from the standard input any more");
            }
            break;
        }
        buffer_.append(std::string_view(chunk.data(), static_cast<std::size_t>(count)));
    }
    return buffer_.take_lines();
}

}  // namespace manifold
