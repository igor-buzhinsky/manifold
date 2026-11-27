#include "party/spells.hpp"

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(SpellsTest, KeysRoundTrip) {
    for (const Spell spell : kAllSpells) {
        EXPECT_EQ(parse_spell_key(spell_key(spell)), spell);
    }
    EXPECT_STREQ(spell_key(Spell::kCandlelight), "candlelight");
    EXPECT_FALSE(parse_spell_key("Levitate").has_value());
    EXPECT_FALSE(parse_spell_key("").has_value());
}

TEST(SpellsTest, DefaultNamesCostsAndDurations) {
    const MagicConfig config;
    EXPECT_EQ(config.spell(Spell::kCandlelight), (SpellType{.name = "Candlelight", .cost = 5.0f, .duration = 60.0f}));
    EXPECT_EQ(config.spell(Spell::kLevitate), (SpellType{.name = "Levitate", .cost = 10.0f, .duration = 60.0f}));
    EXPECT_FLOAT_EQ(config.max_mana, 100.0f);
    EXPECT_FLOAT_EQ(config.mana_regeneration, 1.0f);
}

TEST(SpellsTest, CastingAndRegainingFollowTheConfig) {
    MagicConfig config;
    config.max_mana = 20.0f;
    config.spell(Spell::kLevitate) = {.name = "Float", .cost = 15.0f, .duration = 4.0f};
    PartyMagic magic = PartyMagic::fresh(config);
    EXPECT_EQ(magic, (PartyMagic{.mana = 20.0f, .max_mana = 20.0f, .spells = {}}));
    EXPECT_TRUE(magic.cast(Spell::kLevitate, config));
    EXPECT_FLOAT_EQ(magic.mana, 5.0f);
    EXPECT_FALSE(magic.cast(Spell::kLevitate, config));
    magic.advance(3.0f, 2.0f);
    EXPECT_FLOAT_EQ(magic.mana, 11.0f);
    EXPECT_TRUE(magic.active(Spell::kLevitate));
    magic.advance(1.0f, 2.0f);
    EXPECT_FALSE(magic.active(Spell::kLevitate));
}

TEST(SpellsTest, CastingTakesManaAndFailsWithoutEnough) {
    PartyMagic magic;
    EXPECT_TRUE(magic.cast(Spell::kLevitate));
    EXPECT_FLOAT_EQ(magic.mana, 90.0f);
    EXPECT_TRUE(magic.active(Spell::kLevitate));
    EXPECT_FALSE(magic.active(Spell::kCandlelight));
    magic.mana = 4.5f;
    EXPECT_FALSE(magic.cast(Spell::kCandlelight));
    EXPECT_FLOAT_EQ(magic.mana, 4.5f);
    EXPECT_FALSE(magic.active(Spell::kCandlelight));
    magic.mana = 5.0f;
    EXPECT_TRUE(magic.cast(Spell::kCandlelight));
    EXPECT_FLOAT_EQ(magic.mana, 0.0f);
}

TEST(SpellsTest, CastingAgainRestartsTheDurationInPlace) {
    PartyMagic magic;
    magic.cast(Spell::kLevitate);
    magic.cast(Spell::kCandlelight);
    magic.advance(20.0f);
    EXPECT_TRUE(magic.cast(Spell::kLevitate));
    // The duration starts again rather than adding to what was left, and the order of first casts is kept.
    ASSERT_EQ(magic.spells.size(), 2U);
    EXPECT_EQ(magic.spells[0], (ActiveSpell{.spell = Spell::kLevitate, .remaining = 60.0f}));
    EXPECT_EQ(magic.spells[1], (ActiveSpell{.spell = Spell::kCandlelight, .remaining = 40.0f}));
}

TEST(SpellsTest, AdvanceRegainsManaUpToTheMaximumAndEndsSpells) {
    PartyMagic magic;
    magic.cast(Spell::kCandlelight);
    magic.advance(2.5f);
    EXPECT_FLOAT_EQ(magic.mana, 97.5f);
    magic.advance(10.0f);
    EXPECT_FLOAT_EQ(magic.mana, 100.0f);
    EXPECT_FLOAT_EQ(magic.spells[0].remaining, 47.5f);
    magic.advance(47.0f);
    EXPECT_TRUE(magic.active(Spell::kCandlelight));
    magic.advance(0.5f);
    EXPECT_FALSE(magic.active(Spell::kCandlelight));
    EXPECT_TRUE(magic.spells.empty());
}

TEST(SpellsTest, SetManaIsClampedAndFractionFollows) {
    PartyMagic magic;
    magic.set_mana(-5.0f);
    EXPECT_FLOAT_EQ(magic.mana, 0.0f);
    EXPECT_FLOAT_EQ(magic.mana_fraction(), 0.0f);
    magic.set_mana(250.0f);
    EXPECT_FLOAT_EQ(magic.mana, 100.0f);
    magic.set_mana(25.0f);
    EXPECT_FLOAT_EQ(magic.mana_fraction(), 0.25f);
    magic.max_mana = 0.0f;
    EXPECT_FLOAT_EQ(magic.mana_fraction(), 0.0f);
}

}  // namespace
}  // namespace manifold
