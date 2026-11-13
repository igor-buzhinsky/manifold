#include "console/console.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

using Words = std::vector<std::string>;

TEST(ConsoleTest, ParsesCommandsSeparatedBySemicolonsAndLines) {
    const std::vector<Words> commands = parse_commands("  camera 1 2\t3 ;;wait 2\n\nquit;\r\n");
    ASSERT_EQ(commands.size(), 3U);
    EXPECT_EQ(commands[0], (Words{"camera", "1", "2", "3"}));
    EXPECT_EQ(commands[1], (Words{"wait", "2"}));
    EXPECT_EQ(commands[2], (Words{"quit"}));
    EXPECT_TRUE(parse_commands(" ; \n ").empty());
}

TEST(ConsoleTest, ParsesArguments) {
    EXPECT_FLOAT_EQ(parse_float_argument("-1.5"), -1.5f);
    EXPECT_FLOAT_EQ(parse_float_argument("3"), 3.0f);
    EXPECT_THROW(parse_float_argument("1.5x"), CommandError);
    EXPECT_THROW(parse_float_argument(""), CommandError);
    EXPECT_THROW(parse_float_argument("inf"), CommandError);
    EXPECT_FLOAT_EQ(parse_float_argument("+2.5"), 2.5f);
    EXPECT_THROW(parse_float_argument("+-2"), CommandError);
    EXPECT_EQ(parse_int_argument("-42"), -42);
    EXPECT_EQ(parse_int_argument("+7"), 7);
    EXPECT_THROW(parse_int_argument("4.2"), CommandError);
    EXPECT_THROW(parse_int_argument("99999999999"), CommandError);
    EXPECT_TRUE(parse_switch_argument("on"));
    EXPECT_FALSE(parse_switch_argument("off"));
    EXPECT_THROW(parse_switch_argument("yes"), CommandError);
}

TEST(ConsoleTest, LineBufferReturnsOnlyCompleteLines) {
    LineBuffer buffer;
    buffer.append("wait 1\nqu");
    EXPECT_EQ(buffer.take_lines(), (Words{"wait 1"}));
    EXPECT_TRUE(buffer.take_lines().empty());
    buffer.append("it\n\n");
    EXPECT_EQ(buffer.take_lines(), (Words{"quit", ""}));
}

TEST(ConsoleTest, RunsCommandsByName) {
    Console console;
    Words received;
    console.add("echo", "echo WORDS...", "Repeats the words.",
                [&](std::span<const std::string> arguments) { received.assign(arguments.begin(), arguments.end()); });
    console.run(Words{"echo", "a", "b"});
    EXPECT_EQ(received, (Words{"a", "b"}));
    EXPECT_THROW(console.run(Words{"ecco"}), CommandError);
    EXPECT_THROW(console.add("echo", "echo", "Again.", [](std::span<const std::string>) {}), std::invalid_argument);
}

TEST(ConsoleTest, HelpListsAllCommands) {
    Console console;
    console.add("quit", "quit", "Quits.", [](std::span<const std::string>) {});
    const std::string help = console.help();
    EXPECT_NE(help.find("help"), std::string::npos);
    EXPECT_NE(help.find("Quits."), std::string::npos);
    EXPECT_NO_THROW(console.run(Words{"help"}));
    EXPECT_THROW(console.run(Words{"help", "me"}), CommandError);
}

/// A console with commands that record their runs, and `wait`.
struct RecordingConsole {
    Console console;
    CommandQueue queue;
    Words runs;

    RecordingConsole() {
        for (const std::string name : {"a", "b", "c"}) {
            console.add(name, name, "Records its name.",
                        [this, name](std::span<const std::string>) { runs.push_back(name); });
        }
        console.add("wait", "wait FRAMES", "Waits.",
                    [this](std::span<const std::string> arguments) { queue.wait(parse_int_argument(arguments[0])); });
        console.add("fail", "fail", "Fails.", [](std::span<const std::string>) { throw CommandError("failed"); });
    }
};

TEST(CommandQueueTest, RunsCommandsInOrderUntilOneWaits) {
    RecordingConsole recording;
    recording.queue.add("a; wait 2; b; wait 0; c", true);
    recording.queue.run_frame(recording.console);
    EXPECT_EQ(recording.runs, (Words{"a"}));
    recording.queue.run_frame(recording.console);  // One frame later.
    EXPECT_EQ(recording.runs, (Words{"a"}));
    recording.queue.run_frame(recording.console);  // Two frames later.
    EXPECT_EQ(recording.runs, (Words{"a", "b", "c"}));
    EXPECT_EQ(recording.queue.size(), 0U);
}

TEST(CommandQueueTest, ThrowsErrorsOnlyOfCommandsThatStopOnError) {
    RecordingConsole recording;
    recording.queue.add("a; fail; b", false);
    EXPECT_NO_THROW(recording.queue.run_frame(recording.console));
    EXPECT_EQ(recording.runs, (Words{"a", "b"}));

    recording.queue.add("fail 1; c", true);
    try {
        recording.queue.run_frame(recording.console);
        FAIL() << "expected a CommandError";
    } catch (const CommandError& error) {
        EXPECT_EQ(std::string(error.what()), "fail 1: failed");
    }
    EXPECT_EQ(recording.queue.size(), 1U);  // c is still waiting.
}

TEST(CommandQueueTest, TellsWhetherTheRunningCommandStopsOnError) {
    CommandQueue queue;
    Console console;
    std::vector<bool> seen;
    console.add("check", "check", "Records the flag.",
                [&](std::span<const std::string>) { seen.push_back(queue.running_stops_on_error()); });
    queue.add("check", true);
    queue.add("check", false);
    queue.run_frame(console);
    EXPECT_EQ(seen, (std::vector<bool>{true, false}));
}

TEST(CommandQueueTest, ClearRemovesWaitingCommands) {
    RecordingConsole recording;
    recording.queue.add("a; wait 3; b; c", true);
    recording.queue.run_frame(recording.console);
    EXPECT_EQ(recording.queue.clear(), 2U);
    recording.queue.add("c", true);
    recording.queue.run_frame(recording.console);  // No longer waits.
    EXPECT_EQ(recording.runs, (Words{"a", "c"}));
}

}  // namespace
}  // namespace manifold
