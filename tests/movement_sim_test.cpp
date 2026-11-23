#include "sim/movement_sim.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "level/level.hpp"

namespace manifold {
namespace {

/// A short run: 2 seeds of each layout, 5 s each.
MovementSimConfig short_config() {
    MovementSimConfig config;
    config.seeds = 2;
    config.seconds = 5.0f;
    return config;
}

TEST(MovementSimTest, AShortRunOfBothLayoutsKeepsTheInvariants) {
    const MovementSimReport report = run_movement_sim(short_config());
    EXPECT_TRUE(report.ok()) << format_movement_sim_report(report);
    EXPECT_EQ(report.frames, 4 * 300);
    EXPECT_GE(report.levels, 4);
    EXPECT_GT(report.jumps, 0);
}

TEST(MovementSimTest, TheSameConfigGivesTheSameReport) {
    const MovementSimReport first = run_movement_sim(short_config());
    const MovementSimReport second = run_movement_sim(short_config());
    EXPECT_EQ(format_movement_sim_report(first), format_movement_sim_report(second));
}

TEST(MovementSimTest, TheReportListsTheCountsAndExamples) {
    MovementSimReport report;
    report.frames = 100;
    report.levels = 1;
    EXPECT_TRUE(report.ok());
    EXPECT_NE(format_movement_sim_report(report).find("OK"), std::string::npos);

    report.counts.at(static_cast<std::size_t>(MovementViolation::kOverlap)) = 3;
    report.examples.push_back({.violation = MovementViolation::kOverlap,
                               .layout = LayoutKind::kRooms,
                               .seed = 7,
                               .level_number = 1,
                               .frame = 42,
                               .position = {1.0f, 2.0f, 3.0f},
                               .detail = "0.0100 m deep"});
    EXPECT_FALSE(report.ok());
    const std::string text = format_movement_sim_report(report);
    EXPECT_NE(text.find("overlap: 3"), std::string::npos) << text;
    EXPECT_NE(text.find("overlap: layout rooms, seed 7, level 1, frame 42, at (1.00, 2.00, 3.00): 0.0100 m deep"),
              std::string::npos)
        << text;
    EXPECT_NE(text.find("FAILED"), std::string::npos) << text;
}

TEST(MovementSimTest, RejectsConfigsThatCantRun) {
    const auto with = [](auto change) {
        MovementSimConfig config = short_config();
        change(config);
        return config;
    };
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.seeds = 0; })), std::invalid_argument);
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.dt = 0.0f; })), std::invalid_argument);
    // So large that a jump, a single press, would be held in every frame.
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.dt = 0.5f; })), std::invalid_argument);
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.dt = std::nanf(""); })), std::invalid_argument);
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.seconds = std::nanf(""); })),
                 std::invalid_argument);
    EXPECT_THROW(run_movement_sim(with([](MovementSimConfig& c) { c.seconds = 0.001f; })), std::invalid_argument);
}

TEST(MovementSimTest, EveryViolationHasAName) {
    for (std::size_t i = 0; i < kMovementViolationCount; ++i) {
        EXPECT_NE(movement_violation_name(static_cast<MovementViolation>(i)), "unknown");
    }
}

}  // namespace
}  // namespace manifold
