#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace manifold {

/// A console command that can't be run: it is unknown, or its arguments are wrong.
class CommandError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Splits console text into commands, each a list of words.
///
/// Commands are separated by `;` or line breaks, words by spaces or tabs. Empty commands are left out.
/// There is no quoting, so a word can't contain spaces.
std::vector<std::vector<std::string>> parse_commands(std::string_view text);

/// Parses a decimal number, such as `-1.5`, given as a command argument.
///
/// \throws CommandError if `text` is not a finite number.
float parse_float_argument(const std::string& text);

/// Parses a decimal integer given as a command argument.
///
/// \throws CommandError if `text` is not an integer that fits into an int.
int parse_int_argument(const std::string& text);

/// Parses `on` or `off` given as a command argument.
///
/// \throws CommandError if `text` is something else.
bool parse_switch_argument(const std::string& text);

/// Collects text that arrives in pieces, e.g. from a terminal, and splits it into lines.
class LineBuffer {
public:
    /// Adds text after what was added before.
    void append(std::string_view text) { pending_ += text; }

    /// Removes the complete lines, those that end with a line break, and returns them without it.
    std::vector<std::string> take_lines();

private:
    std::string pending_;
};

/// Console commands, by name.
///
/// The command `help`, which lists all commands, is always there.
class Console {
public:
    /// Runs a command, given its arguments without the name.
    ///
    /// Throws CommandError if the arguments are wrong.
    using Handler = std::function<void(std::span<const std::string> arguments)>;

    Console();

    /// Adds a command.
    ///
    /// \param name    What to type to run it, e.g. `screenshot`.
    /// \param usage   The name and the arguments it takes, e.g. `screenshot FILE [WIDTH HEIGHT]`.
    /// \param help    What it does, in one sentence.
    /// \param handler Runs it.
    /// \throws std::invalid_argument if there is already a command with this name.
    void add(const std::string& name, std::string usage, std::string help, Handler handler);

    /// Runs a command.
    ///
    /// \param words The name of the command, then its arguments.
    /// \throws CommandError if there is no such command or its arguments are wrong.
    void run(std::span<const std::string> words) const;

    /// Lists all commands, one per line with its usage and help, in alphabetical order.
    std::string help() const;

private:
    struct Command {
        std::string usage;
        std::string help;
        Handler handler;
    };

    std::map<std::string, Command, std::less<>> commands_;
};

/// Console commands waiting to run, in order, possibly a few frames later.
class CommandQueue {
public:
    /// Adds the commands in `text`, see parse_commands(), after those already waiting.
    ///
    /// \param text          The commands.
    /// \param stop_on_error Whether an error in one of these commands is thrown by run_frame(), e.g. for a
    ///                      script that makes no sense after a failed command, or only logged, e.g. for a
    ///                      command typed by hand.
    void add(std::string_view text, bool stop_on_error);

    /// Makes the commands after the one that is running run `frames` frames later.
    ///
    /// With 0, they run in the same frame, with 1 in the next frame, and so on.
    void wait(int frames) { frames_to_wait_ = frames; }

    /// Removes the commands that haven't run yet.
    ///
    /// \return How many commands were removed.
    std::size_t clear();

    /// Whether the command that is running was added with `stop_on_error`, see add().
    ///
    /// A command whose work is done later, e.g. by the main loop, can use this to decide what to do when
    /// that work fails.
    bool running_stops_on_error() const { return running_stops_on_error_; }

    /// Runs the commands that are due in this frame: one after another, until none are left or one waits.
    ///
    /// Call it once per frame.
    ///
    /// \throws CommandError if a command added with `stop_on_error` fails, with the command in the message.
    ///         The commands after it stay in the queue. Errors of other commands are logged.
    void run_frame(const Console& console);

    /// Number of commands that haven't run yet.
    std::size_t size() const { return commands_.size(); }

private:
    struct Entry {
        std::vector<std::string> words;
        bool stop_on_error = false;
    };

    std::deque<Entry> commands_;
    int frames_to_wait_ = 0;
    bool running_stops_on_error_ = false;
};

}  // namespace manifold
