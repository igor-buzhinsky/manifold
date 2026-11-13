#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/text_field.hpp"
#include "util/log.hpp"

namespace manifold {

/// Keys of the console window, besides those that edit its input line (see EditKey).
enum class ConsoleKey : std::uint8_t {
    kSubmit,          ///< Runs the input line (Enter).
    kHistoryBack,     ///< Shows the previous command of the history in the input line (Up).
    kHistoryForward,  ///< Shows the next command of the history, or what was typed before (Down).
    kScrollUp,        ///< Shows older output (Page Up).
    kScrollDown,      ///< Shows newer output (Page Down).
};

/// The in-game console: a window over the top half of the picture with the log output and an input line.
///
/// It only keeps its state and lays itself out, see build_mesh(). The caller passes it keys and typed text,
/// runs the commands it returns, and draws it.
class ConsoleWindow {
public:
    /// Most lines of output kept. Older lines are dropped.
    static constexpr std::size_t kMaxOutputLines = 1000;
    /// Most commands kept in the history.
    static constexpr std::size_t kMaxHistory = 100;
    /// How many lines of output a scroll key moves.
    static constexpr std::size_t kScrollStep = 10;

    bool is_open() const { return open_; }
    void set_open(bool open) { open_ = open; }

    /// The input line, which typed text and EditKey go to.
    TextField& input() { return input_; }
    const TextField& input() const { return input_; }

    /// Adds a log message to the output, one line per line of the message.
    void add_output(LogLevel level, std::string_view message);

    /// Handles a key.
    ///
    /// \return For kSubmit, the command to run, which is also added to the output and the history, unless
    ///         the input line is empty or only spaces. Otherwise nothing.
    std::optional<std::string> handle(ConsoleKey key);

    /// Number of output lines that the view is scrolled up from the newest one.
    std::size_t scroll() const { return scroll_; }

    /// Lays out the window over a picture of `size` pixels: a dark background, as much of the output as fits
    /// with long lines wrapped, and the input line with its cursor at the bottom.
    OverlayMesh build_mesh(const PixelSize& size) const;

private:
    /// What a line of the output is.
    enum class LineKind : std::uint8_t { kInfo, kWarning, kError, kCommand };

    struct Line {
        std::string text;
        LineKind kind = LineKind::kInfo;
    };

    void add_line(std::string text, LineKind kind);

    bool open_ = false;
    TextField input_;
    std::deque<Line> output_;
    std::size_t scroll_ = 0;
    std::vector<std::string> history_;
    std::optional<std::size_t> history_index_;  ///< The history entry shown in the input line, if any.
    std::string draft_;                         ///< What was typed before going back in the history.
};

}  // namespace manifold
