#include "party/spells.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace manifold {

const char* spell_name(Spell spell) {
    switch (spell) {
        case Spell::kCandlelight:
            return "candlelight";
        case Spell::kLevitate:
            return "levitate";
    }
    return "unknown";
}

const char* spell_title(Spell spell) {
    switch (spell) {
        case Spell::kCandlelight:
            return "Candlelight";
        case Spell::kLevitate:
            return "Levitate";
    }
    return "Unknown";
}

std::optional<Spell> parse_spell_name(std::string_view name) {
    for (const Spell spell : kAllSpells) {
        if (name == spell_name(spell)) {
            return spell;
        }
    }
    return std::nullopt;
}

float spell_cost(Spell spell) {
    switch (spell) {
        case Spell::kCandlelight:
            return 5.0f;
        case Spell::kLevitate:
            return 10.0f;
    }
    return 0.0f;
}

float spell_duration(Spell /*spell*/) { return 60.0f; }

bool PartyMagic::cast(Spell spell) {
    const float cost = spell_cost(spell);
    if (mana < cost) {
        return false;
    }
    mana -= cost;
    const float duration = spell_duration(spell);
    const auto found = std::ranges::find(spells, spell, &ActiveSpell::spell);
    if (found != spells.end()) {
        found->remaining = duration;
    } else {
        spells.push_back({.spell = spell, .remaining = duration});
    }
    return true;
}

void PartyMagic::advance(float dt) {
    mana = std::min(mana + (kManaRegeneration * dt), max_mana);
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
