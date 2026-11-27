#include "audio/spell_sounds.hpp"

#include <string>

#include <gtest/gtest.h>

#include "audio/sound.hpp"
#include "party/spells.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {
namespace {

/// Loads the spell sounds of `json`, a mapping of the game's assets, and returns what was logged in `log`.
SpellSounds load(const std::string& json, std::string& log) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping = AssetMapping::parse(json, MANIFOLD_ASSETS_DIR, "test.json");
    const SpellSounds sounds(mapping);
    log = testing::internal::GetCapturedStderr();
    return sounds;
}

TEST(SpellSoundsTest, LoadsTheSoundOfEachSpell) {
    std::string log;
    const SpellSounds sounds = load(R"({"SPELL_SOUNDS": {
        "candlelight": "sounds/door_open/kenney-rpg-audio/doorOpen_1.ogg",
        "levitate": "sounds/door_open/kenney-rpg-audio/doorOpen_2.ogg"
    }})",
                                    log);
    const Sound* candlelight = sounds.get(Spell::kCandlelight);
    const Sound* levitate = sounds.get(Spell::kLevitate);
    ASSERT_NE(candlelight, nullptr);
    ASSERT_NE(levitate, nullptr);
    EXPECT_NE(candlelight->samples, levitate->samples);
    EXPECT_NE(log.find("[info] 2 of 2 SPELL_SOUNDS loaded"), std::string::npos) << log;
    EXPECT_EQ(log.find("Spell"), std::string::npos) << log;
}

TEST(SpellSoundsTest, SpellWithoutAUsableSoundHasNoneAfterAWarning) {
    std::string log;
    const SpellSounds sounds = load(R"({"SPELL_SOUNDS": {
        "levitate": "sounds/missing.ogg",
        "fireball": "sounds/door_open/kenney-rpg-audio/doorOpen_1.ogg"
    }})",
                                    log);
    EXPECT_EQ(sounds.get(Spell::kCandlelight), nullptr);
    EXPECT_EQ(sounds.get(Spell::kLevitate), nullptr);
    EXPECT_NE(log.find("[warning] Spell candlelight has no sound in SPELL_SOUNDS"), std::string::npos) << log;
    EXPECT_NE(log.find("missing.ogg"), std::string::npos) << log;
    EXPECT_NE(log.find("[warning] SPELL_SOUNDS has a sound for fireball, which is no spell"), std::string::npos) << log;
    EXPECT_NE(log.find("[info] 0 of 2 SPELL_SOUNDS loaded"), std::string::npos) << log;
}

TEST(GameSpellSoundsTest, EverySpellOfTheGameHasASound) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping = AssetMapping::load(MANIFOLD_ASSETS_DIR);
    const SpellSounds sounds(mapping);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
    for (const Spell spell : kAllSpells) {
        EXPECT_NE(sounds.get(spell), nullptr) << spell_key(spell);
    }
}

}  // namespace
}  // namespace manifold
