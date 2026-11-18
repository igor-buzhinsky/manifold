#include "party/party.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(PartyTest, GenderNamesRoundTrip) {
    for (const Gender gender : {Gender::kMale, Gender::kFemale}) {
        EXPECT_EQ(parse_gender_name(gender_name(gender)), gender);
    }
    EXPECT_FALSE(parse_gender_name("any").has_value());
    EXPECT_FALSE(parse_gender_name("Male").has_value());
}

TEST(PartyTest, HpFractionIsClamped) {
    EXPECT_FLOAT_EQ((Character{.portrait = "", .gender = Gender::kMale, .hp = 25, .max_hp = 100}).hp_fraction(), 0.25f);
    EXPECT_FLOAT_EQ((Character{.portrait = "", .gender = Gender::kMale, .hp = 0, .max_hp = 100}).hp_fraction(), 0.0f);
    EXPECT_FLOAT_EQ((Character{.portrait = "", .gender = Gender::kMale, .hp = 150, .max_hp = 100}).hp_fraction(), 1.0f);
    EXPECT_FLOAT_EQ((Character{.portrait = "", .gender = Gender::kMale, .hp = 5, .max_hp = 0}).hp_fraction(), 0.0f);
}

/// Portraits of each group, two of men, one of women and one of anyone.
std::vector<PortraitFile> make_portraits() {
    return {{.path = "any/skeleton.png", .gender = std::nullopt},
            {.path = "female/a.png", .gender = Gender::kFemale},
            {.path = "male/b.png", .gender = Gender::kMale},
            {.path = "male/c.png", .gender = Gender::kMale}};
}

TEST(PartyTest, PartyHasDifferentPortraitsWithTheirGendersAndFullHp) {
    const std::vector<PortraitFile> portraits = make_portraits();
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 80}, seed);
        ASSERT_EQ(party.size(), 3U);
        std::set<std::string> used;
        for (const Character& character : party) {
            EXPECT_TRUE(used.insert(character.portrait).second) << character.portrait;
            EXPECT_EQ(character.hp, 80);
            EXPECT_EQ(character.max_hp, 80);
            if (character.portrait.starts_with("male/")) {
                EXPECT_EQ(character.gender, Gender::kMale);
            } else if (character.portrait.starts_with("female/")) {
                EXPECT_EQ(character.gender, Gender::kFemale);
            } else {
                EXPECT_EQ(character.portrait, "any/skeleton.png");
            }
        }
    }
}

TEST(PartyTest, PartyDependsOnlyOnTheSeedAndReachesEveryPortraitAndGender) {
    const std::vector<PortraitFile> portraits = make_portraits();
    std::set<std::string> first_portraits;
    std::set<Gender> skeleton_genders;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 10}, seed);
        EXPECT_EQ(party, make_party(portraits, {.size = 3, .max_hp = 10}, seed));
        first_portraits.insert(party[0].portrait);
        for (const Character& character : party) {
            if (character.portrait == "any/skeleton.png") {
                skeleton_genders.insert(character.gender);
            }
        }
    }
    EXPECT_EQ(first_portraits.size(), portraits.size());
    EXPECT_EQ(skeleton_genders.size(), 2U);
}

TEST(PartyTest, TooFewPortraitsLeaveTheLastCharactersWithout) {
    const std::vector<PortraitFile> portraits = {{.path = "female/a.png", .gender = Gender::kFemale}};
    const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 10}, 1);
    ASSERT_EQ(party.size(), 3U);
    EXPECT_EQ(party[0].portrait, "female/a.png");
    EXPECT_EQ(party[0].gender, Gender::kFemale);
    EXPECT_TRUE(party[1].portrait.empty());
    EXPECT_TRUE(party[2].portrait.empty());
    EXPECT_EQ(make_party({}, {.size = 2, .max_hp = 10}, 1).size(), 2U);
}

/// Creates an empty directory for one test and removes it afterwards.
class PortraitFilesTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    /// Writes a small file at `relative` inside the test directory, creating directories as needed.
    void write_file(const std::string& relative) const {
        const std::filesystem::path path = dir_ / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "data";
    }

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

TEST_F(PortraitFilesTest, FindsThePortraitsOfEachGroupSortedWithRelativePaths) {
    write_file("male/met/b.png");
    write_file("female/a.png");
    write_file("any/rijks/deep/c.PNG");
    write_file("male/met/README.md");
    write_file("other/d.png");
    write_file("e.png");
    EXPECT_EQ(find_portraits(dir()), (std::vector<PortraitFile>{
                                         {.path = "any/rijks/deep/c.PNG", .gender = std::nullopt},
                                         {.path = "female/a.png", .gender = Gender::kFemale},
                                         {.path = "male/met/b.png", .gender = Gender::kMale},
                                     }));
    EXPECT_TRUE(find_portraits(dir() / "missing").empty());
}

}  // namespace
}  // namespace manifold
