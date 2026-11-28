#include "party/party.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/log.hpp"
#include "util/random.hpp"

namespace manifold {

namespace {

/// A random gender, each with a chance of 50%.
Gender random_gender(Rng& random) { return random_chance(random, 0.5) ? Gender::kFemale : Gender::kMale; }

/// The gender of the characters that the portraits of `key` are for, or nothing for any gender.
std::optional<Gender> portrait_gender(AssetKey key) {
    if (key == AssetKey::kMalePortraits) {
        return Gender::kMale;
    }
    if (key == AssetKey::kFemalePortraits) {
        return Gender::kFemale;
    }
    return std::nullopt;
}

}  // namespace

const char* gender_name(Gender gender) {
    switch (gender) {
        case Gender::kMale:
            return "male";
        case Gender::kFemale:
            return "female";
    }
    return "unknown";
}

std::optional<Gender> parse_gender_name(std::string_view name) {
    for (const Gender gender : {Gender::kMale, Gender::kFemale}) {
        if (name == gender_name(gender)) {
            return gender;
        }
    }
    return std::nullopt;
}

float Character::hp_fraction() const {
    if (max_hp <= 0) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(hp) / static_cast<float>(max_hp), 0.0f, 1.0f);
}

bool party_defeated(std::span<const Character> party) {
    return !party.empty() && std::ranges::all_of(party, [](const Character& character) { return character.dead(); });
}

bool is_portrait_key(AssetKey key) {
    return key == AssetKey::kMalePortraits || key == AssetKey::kFemalePortraits || key == AssetKey::kAnyPortraits;
}

std::vector<Character> make_party(const AssetCounts& portraits, const PartyConfig& config, std::uint64_t seed) {
    const std::size_t size = config.size;
    Rng random(seed);
    std::vector<Portrait> shuffled;
    for (const AssetKey key : {AssetKey::kMalePortraits, AssetKey::kFemalePortraits, AssetKey::kAnyPortraits}) {
        for (AssetIndex index = 0; index < portraits.of(key); ++index) {
            shuffled.push_back({.key = key, .index = index});
        }
    }
    random_shuffle(random, shuffled);
    if (shuffled.size() < size) {
        log_warning("There are {} portraits for a party of {}, so {} character(s) get none", shuffled.size(), size,
                    size - shuffled.size());
    }
    std::vector<Character> party;
    party.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        Character character{.portrait = {}, .gender = Gender::kMale, .hp = config.max_hp, .max_hp = config.max_hp};
        std::optional<Gender> gender;
        if (i < shuffled.size()) {
            character.portrait = shuffled[i];
            gender = portrait_gender(shuffled[i].key);
        }
        character.gender = gender ? *gender : random_gender(random);
        party.push_back(character);
    }
    return party;
}

}  // namespace manifold
