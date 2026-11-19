#include "party/party.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "util/asset_files.hpp"
#include "util/log.hpp"
#include "util/random.hpp"

namespace manifold {

namespace {

/// A random gender, each with a chance of 50%.
Gender random_gender(Rng& random) { return random_chance(random, 0.5) ? Gender::kFemale : Gender::kMale; }

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

std::vector<PortraitFile> find_portraits(const std::filesystem::path& root) {
    const std::array<std::pair<std::string_view, std::optional<Gender>>, 3> groups{{
        {kMalePortraitsDir, Gender::kMale},
        {kFemalePortraitsDir, Gender::kFemale},
        {kAnyPortraitsDir, std::nullopt},
    }};
    std::vector<PortraitFile> portraits;
    for (const auto& [dir, gender] : groups) {
        for (const std::filesystem::path& path : find_asset_files(root / dir, ".png")) {
            portraits.push_back({.path = path.lexically_relative(root).generic_string(), .gender = gender});
        }
    }
    std::ranges::sort(portraits, {}, &PortraitFile::path);
    return portraits;
}

std::vector<Character> make_party(std::span<const PortraitFile> portraits, const PartyConfig& config,
                                  std::uint64_t seed) {
    const std::size_t size = config.size;
    Rng random(seed);
    std::vector<PortraitFile> shuffled(portraits.begin(), portraits.end());
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
            character.portrait = shuffled[i].path;
            gender = shuffled[i].gender;
        }
        character.gender = gender ? *gender : random_gender(random);
        party.push_back(std::move(character));
    }
    return party;
}

}  // namespace manifold
