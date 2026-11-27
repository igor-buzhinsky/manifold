#include "party/spells.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace manifold {

const char* spell_key(Spell spell) {
    switch (spell) {
        case Spell::kCandlelight:
            return "candlelight";
        case Spell::kLevitate:
            return "levitate";
    }
    return "unknown";
}

std::optional<Spell> parse_spell_key(std::string_view key) {
    for (const Spell spell : kAllSpells) {
        if (key == spell_key(spell)) {
            return spell;
        }
    }
    return std::nullopt;
}

PartyMagic PartyMagic::fresh(const MagicConfig& config) {
    return {.mana = config.max_mana, .max_mana = config.max_mana, .spells = {}};
}

bool PartyMagic::cast(Spell spell, const MagicConfig& config) {
    const SpellType& type = config.spell(spell);
    const float cost = type.cost;
    if (mana < cost) {
        return false;
    }
    mana -= cost;
    const float duration = type.duration;
    const auto found = std::ranges::find(spells, spell, &ActiveSpell::spell);
    if (found != spells.end()) {
        found->remaining = duration;
    } else {
        spells.push_back({.spell = spell, .remaining = duration});
    }
    return true;
}

void PartyMagic::advance(float dt, float regeneration) {
    mana = std::min(mana + (regeneration * dt), max_mana);
    for (ActiveSpell& active : spells) {
        active.remaining -= dt;
    }
    std::erase_if(spells, [](const ActiveSpell& active) { return active.remaining <= 0.0f; });
}

bool PartyMagic::active(Spell spell) const {
    return std::ranges::find(spells, spell, &ActiveSpell::spell) != spells.end();
}

void PartyMagic::set_mana(float value) { mana = std::clamp(value, 0.0f, max_mana); }

float PartyMagic::mana_fraction() const {
    if (max_mana <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(mana / max_mana, 0.0f, 1.0f);
}

}  // namespace manifold
