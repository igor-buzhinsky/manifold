#include "ui/party_hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "party/party.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// Where a portrait has no image: nearly black, so that the empty place still shows.
constexpr glm::vec4 kEmptyPortraitColor{0.06f, 0.06f, 0.07f, 1.0f};
/// The part of a bar of hit points that is lost, and the part that is left.
constexpr glm::vec4 kHpBarBackColor{0.15f, 0.02f, 0.02f, 1.0f};
constexpr glm::vec4 kHpBarColor{0.85f, 0.08f, 0.06f, 1.0f};

}  // namespace

std::vector<CharacterPlacement> layout_party_hud(const PixelSize& picture, std::size_t count) {
    const float scale = ui_scale(picture);
    const glm::vec2 portrait_size = glm::vec2(kHudPortraitWidth, kHudPortraitHeight) * scale;
    const float bar_height = kHudBarHeight * scale;
    const float bar_top = static_cast<float>(picture.height) - ((kHudMargin + kHudBarHeight) * scale);
    const float portrait_top = bar_top - (kHudBarGap * scale) - portrait_size.y;
    std::vector<CharacterPlacement> placements;
    placements.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float left = (kHudMargin + (static_cast<float>(i) * (kHudPortraitWidth + kHudPortraitGap))) * scale;
        placements.push_back({.portrait = {.top_left = {left, portrait_top}, .size = portrait_size},
                              .hp_bar = {.top_left = {left, bar_top}, .size = {portrait_size.x, bar_height}}});
    }
    return placements;
}

bool party_hud_contains(const PixelSize& picture, std::size_t count, const glm::vec2& point) {
    const std::vector<CharacterPlacement> placements = layout_party_hud(picture, count);
    if (placements.empty()) {
        return false;
    }
    const glm::vec2 top_left = placements.front().portrait.top_left;
    const Rect& last_bar = placements.back().hp_bar;
    const Rect area{.top_left = top_left, .size = last_bar.top_left + last_bar.size - top_left};
    return area.contains(point);
}

void add_party_hud(OverlayMesh& mesh, std::span<const CharacterPlacement> placements,
                   std::span<const Character> party) {
    const std::size_t count = std::min(placements.size(), party.size());
    for (std::size_t i = 0; i < count; ++i) {
        const CharacterPlacement& placement = placements[i];
        mesh.add_rectangle(placement.portrait.top_left, placement.portrait.size, kEmptyPortraitColor);
        mesh.add_rectangle(placement.hp_bar.top_left, placement.hp_bar.size, kHpBarBackColor);
        // Whole pixels keep the end of the bar sharp, and a character with any hit points left keeps one.
        const float fraction = party[i].hp_fraction();
        const float filled = fraction > 0.0f ? std::max(1.0f, std::round(placement.hp_bar.size.x * fraction)) : 0.0f;
        if (filled > 0.0f) {
            mesh.add_rectangle(placement.hp_bar.top_left, {filled, placement.hp_bar.size.y}, kHpBarColor);
        }
    }
}

}  // namespace manifold
