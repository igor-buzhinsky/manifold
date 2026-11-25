#include "party/party.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"

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
    EXPECT_FLOAT_EQ(
        (Character{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 25, .max_hp = 100}).hp_fraction(), 0.25f);
    EXPECT_FLOAT_EQ(
        (Character{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 0, .max_hp = 100}).hp_fraction(), 0.0f);
    EXPECT_FLOAT_EQ(
        (Character{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 150, .max_hp = 100}).hp_fraction(), 1.0f);
    EXPECT_FLOAT_EQ((Character{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 5, .max_hp = 0}).hp_fraction(),
                    0.0f);
}

/// Portraits of each group: two of men, one of women and one of anyone.
AssetCounts make_portraits() {
    AssetCounts counts;
    counts.set(AssetKey::kMalePortraits, 2);
    counts.set(AssetKey::kFemalePortraits, 1);
    counts.set(AssetKey::kAnyPortraits, 1);
    // Files of other keys are not portraits.
    counts.set(AssetKey::kDoorOpenSounds, 5);
    return counts;
}

TEST(PartyTest, PartyHasDifferentPortraitsWithTheirGendersAndFullHp) {
    const AssetCounts portraits = make_portraits();
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 80}, seed);
        ASSERT_EQ(party.size(), 3U);
        std::set<std::pair<AssetKey, AssetIndex>> used;
        for (const Character& character : party) {
            ASSERT_TRUE(character.portrait.has_value());
            const Portrait& portrait = *character.portrait;
            EXPECT_TRUE(used.insert({portrait.key, portrait.index}).second);
            EXPECT_LT(portrait.index, portraits.of(portrait.key));
            EXPECT_EQ(character.hp, 80);
            EXPECT_EQ(character.max_hp, 80);
            if (portrait.key == AssetKey::kMalePortraits) {
                EXPECT_EQ(character.gender, Gender::kMale);
            } else if (portrait.key == AssetKey::kFemalePortraits) {
                EXPECT_EQ(character.gender, Gender::kFemale);
            } else {
                EXPECT_EQ(portrait.key, AssetKey::kAnyPortraits);
            }
        }
    }
}

TEST(PartyTest, PartyDependsOnlyOnTheSeedAndReachesEveryPortraitAndGender) {
    const AssetCounts portraits = make_portraits();
    std::set<std::pair<AssetKey, AssetIndex>> first_portraits;
    std::set<Gender> any_genders;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 10}, seed);
        EXPECT_EQ(party, make_party(portraits, {.size = 3, .max_hp = 10}, seed));
        first_portraits.insert({party[0].portrait->key, party[0].portrait->index});
        for (const Character& character : party) {
            if (character.portrait->key == AssetKey::kAnyPortraits) {
                any_genders.insert(character.gender);
            }
        }
    }
    EXPECT_EQ(first_portraits.size(), 4U);
    EXPECT_EQ(any_genders.size(), 2U);
}

TEST(PartyTest, TooFewPortraitsLeaveTheLastCharactersWithout) {
    AssetCounts portraits;
    portraits.set(AssetKey::kFemalePortraits, 1);
    testing::internal::CaptureStderr();
    const std::vector<Character> party = make_party(portraits, {.size = 3, .max_hp = 10}, 1);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_NE(log.find("[warning] There are 1 portraits for a party of 3"), std::string::npos) << log;
    ASSERT_EQ(party.size(), 3U);
    EXPECT_EQ(party[0].portrait, (Portrait{.key = AssetKey::kFemalePortraits, .index = 0}));
    EXPECT_EQ(party[0].gender, Gender::kFemale);
    EXPECT_FALSE(party[1].portrait.has_value());
    EXPECT_FALSE(party[2].portrait.has_value());
    testing::internal::CaptureStderr();
    EXPECT_EQ(make_party({}, {.size = 2, .max_hp = 10}, 1).size(), 2U);
    testing::internal::GetCapturedStderr();
}

TEST(PartyTest, PortraitKeysAreThoseOfPortraits) {
    for (const AssetKey key : kAssetKeys) {
        EXPECT_EQ(is_portrait_key(key), asset_key_type(key) == AssetType::kPortrait) << asset_key_name(key);
    }
}

}  // namespace
}  // namespace manifold
