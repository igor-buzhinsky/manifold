#include "party/fall_damage.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "party/party.hpp"
#include "util/random.hpp"

namespace manifold {
namespace {

/// The landing speed of a free fall from `height` metres under Earth gravity.
float speed_of_fall(float height) { return std::sqrt(2.0f * 9.81f * height); }

TEST(FallDamageTest, LandingsUpToTheSafeSpeedDontHurt) {
    const FallDamageConfig config;
    EXPECT_FLOAT_EQ(base_fall_damage(config, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(base_fall_damage(config, 3.0f), 0.0f);  // A jump of 0.5 m.
    EXPECT_FLOAT_EQ(base_fall_damage(config, config.safe_speed), 0.0f);
    Rng rng(1);
    EXPECT_EQ(fall_damage(config, config.safe_speed, rng), 0);
    // No random number was drawn.
    EXPECT_EQ(rng(), Rng(1)());
}

TEST(FallDamageTest, DamageGrowsWithTheEnergyAboveTheSafeSpeed) {
    const FallDamageConfig config{.safe_speed = 8.0f, .full_damage_speed = 18.0f, .spread = 0.0f};
    EXPECT_FLOAT_EQ(base_fall_damage(config, 18.0f), kFullFallDamage);
    // Half the energy between the two.
    EXPECT_NEAR(base_fall_damage(config, std::sqrt((64.0f + 324.0f) / 2.0f)), kFullFallDamage / 2.0f, 1e-3f);
    EXPECT_GT(base_fall_damage(config, 20.0f), kFullFallDamage);
}

TEST(FallDamageTest, DefaultsAreCloseToRealFalls) {
    FallDamageConfig config;
    config.spread = 0.0f;
    // A drop from one layer of rooms to the next, 3.3 m, doesn't hurt, even with a frame of gravity more at 30 fps.
    EXPECT_FLOAT_EQ(base_fall_damage(config, speed_of_fall(3.3f) + (9.81f / 30.0f)), 0.0f);
    // Two layers take about a quarter, 10 m about half, and the height of a debug level kills.
    EXPECT_NEAR(base_fall_damage(config, speed_of_fall(6.6f)), 23.0f, 1.0f);
    EXPECT_NEAR(base_fall_damage(config, speed_of_fall(10.0f)), 49.0f, 1.0f);
    EXPECT_NEAR(base_fall_damage(config, speed_of_fall(16.5f)), 100.0f, 1.0f);
}

TEST(FallDamageTest, SpreadKeepsTheDamageWithinItsRange) {
    const FallDamageConfig config{.safe_speed = 8.0f, .full_damage_speed = 18.0f, .spread = 0.3f};
    Rng rng(7);
    std::set<int> damages;
    for (int i = 0; i < 1000; ++i) {
        const int damage = fall_damage(config, 18.0f, rng);
        EXPECT_GE(damage, 70);
        EXPECT_LE(damage, 130);
        damages.insert(damage);
    }
    EXPECT_EQ(*damages.begin(), 70);
    EXPECT_EQ(*damages.rbegin(), 130);

    const FallDamageConfig exact{.safe_speed = 8.0f, .full_damage_speed = 18.0f, .spread = 0.0f};
    EXPECT_EQ(fall_damage(exact, 18.0f, rng), 100);
}

TEST(FallDamageTest, EachCharacterIsHurtOnItsOwnDownToZero) {
    const FallDamageConfig config{.safe_speed = 8.0f, .full_damage_speed = 18.0f, .spread = 0.3f};
    const auto make = [](int hp) {
        return Character{.portrait = std::nullopt, .gender = Gender::kMale, .hp = hp, .max_hp = 100};
    };
    std::vector<Character> party = {make(100), make(100), make(100), make(100), make(100), make(100)};
    Rng rng(3);
    const int most = apply_fall_damage(party, config, std::sqrt((64.0f + 324.0f) / 2.0f), rng);
    std::set<int> hps;
    int largest = 0;
    for (const Character& character : party) {
        EXPECT_GE(character.hp, 100 - 65);
        EXPECT_LE(character.hp, 100 - 35);
        hps.insert(character.hp);
        largest = std::max(largest, 100 - character.hp);
    }
    EXPECT_GT(hps.size(), 1U);
    EXPECT_EQ(most, largest);

    // A dead character stays at 0 and draws nothing, and the others don't go below 0.
    std::vector<Character> hurt = {make(0), make(10)};
    Rng first(5);
    Rng second(5);
    // The damage is returned in full, also beyond the 10 hit points left.
    EXPECT_EQ(apply_fall_damage(hurt, config, 18.0f, first), fall_damage(config, 18.0f, second));
    EXPECT_EQ(hurt[0].hp, 0);
    EXPECT_EQ(hurt[1].hp, 0);
    EXPECT_EQ(first(), second());

    std::vector<Character> safe = {make(50)};
    EXPECT_EQ(apply_fall_damage(safe, config, 5.0f, rng), 0);
    EXPECT_EQ(safe[0].hp, 50);
}

}  // namespace
}  // namespace manifold
