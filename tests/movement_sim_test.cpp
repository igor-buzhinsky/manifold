#include "sim/movement_sim.hpp"

#include <cstddef>
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

TEST(MovementSimTest, EveryViolationHasAName) {
    for (std::size_t i = 0; i < kMovementViolationCount; ++i) {
        EXPECT_NE(movement_violation_name(static_cast<MovementViolation>(i)), "unknown");
    }
}

}  // namespace
}  // namespace manifold
