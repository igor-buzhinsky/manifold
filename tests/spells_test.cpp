#include "party/spells.hpp"

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(SpellsTest, NamesRoundTrip) {
    for (const Spell spell : kAllSpells) {
        EXPECT_EQ(parse_spell_name(spell_name(spell)), spell);
    }
    EXPECT_STREQ(spell_name(Spell::kCandlelight), "candlelight");
    EXPECT_STREQ(spell_title(Spell::kLevitate), "Levitate");
    EXPECT_FALSE(parse_spell_name("Levitate").has_value());
    EXPECT_FALSE(parse_spell_name("").has_value());
}

TEST(SpellsTest, CostsAndDurations) {
    EXPECT_FLOAT_EQ(spell_cost(Spell::kCandlelight), 5.0f);
    EXPECT_FLOAT_EQ(spell_cost(Spell::kLevitate), 10.0f);
    for (const Spell spell : kAllSpells) {
        EXPECT_FLOAT_EQ(spell_duration(spell), 60.0f);
    }
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
