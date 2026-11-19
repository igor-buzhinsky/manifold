#include "ui/spell_hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "party/spells.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/party_hud.hpp"
#include "ui/rect.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// The box of a spell, and its name.
constexpr glm::vec4 kSpellBoxColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr glm::vec4 kSpellTextColor{0.0f, 0.0f, 0.0f, 1.0f};
/// The part of the bar of mana that is spent, and the part that is left.
constexpr glm::vec4 kManaBarBackColor{0.02f, 0.03f, 0.15f, 1.0f};
constexpr glm::vec4 kManaBarColor{0.15f, 0.3f, 0.95f, 1.0f};

/// Width of the boxes of the spells, in interface pixels: that of the longest name with the padding.
float spell_box_width() {
    float width = 0.0f;
    for (const Spell spell : kAllSpells) {
        width = std::max(width, text_width(spell_title(spell), 1.0f));
    }
    return width + (2.0f * kHudSpellPaddingX);
}

}  // namespace

std::vector<Rect> layout_spell_hud(const PixelSize& picture, std::size_t count) {
    const float scale = ui_scale(picture);
    const glm::vec2 size = glm::vec2(spell_box_width(), kTextLetterHeight + (2.0f * kHudSpellPaddingY)) * scale;
    const float left = static_cast<float>(picture.width) - (kHudMargin * scale) - size.x;
    std::vector<Rect> boxes;
    boxes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float top = (kHudMargin * scale) + (static_cast<float>(i) * (size.y + (kHudSpellGap * scale)));
        boxes.push_back({.top_left = {left, top}, .size = size});
    }
    return boxes;
}

Rect layout_mana_bar(const PixelSize& picture) {
    const float scale = ui_scale(picture);
    const glm::vec2 size = glm::vec2(kHudManaBarWidth, kHudManaBarHeight) * scale;
    const glm::vec2 bottom_right =
        glm::vec2(static_cast<float>(picture.width), static_cast<float>(picture.height)) - (kHudMargin * scale);
    return {.top_left = bottom_right - size, .size = size};
}

bool spell_hud_contains(const PixelSize& picture, const PartyMagic& magic, const glm::vec2& point) {
    if (layout_mana_bar(picture).contains(point)) {
        return true;
    }
    const std::vector<Rect> boxes = layout_spell_hud(picture, magic.spells.size());
    if (boxes.empty()) {
        return false;
    }
    const Rect& last = boxes.back();
    const Rect area{.top_left = boxes.front().top_left, .size = last.top_left + last.size - boxes.front().top_left};
    return area.contains(point);
}

void add_spell_hud(OverlayMesh& mesh, const PixelSize& picture, const PartyMagic& magic) {
    const float scale = ui_scale(picture);
    const std::vector<Rect> boxes = layout_spell_hud(picture, magic.spells.size());
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const Rect& box = boxes[i];
        const char* title = spell_title(magic.spells[i].spell);
        mesh.add_rectangle(box.top_left, box.size, kSpellBoxColor);
        // Centered, rounded down to whole interface pixels so that the font stays sharp.
        const float text_x =
            box.top_left.x + (std::floor((box.size.x - text_width(title, scale)) / 2.0f / scale) * scale);
        mesh.add_text({text_x, box.top_left.y + (kHudSpellPaddingY * scale)}, title, scale, kSpellTextColor);
    }

    const Rect bar = layout_mana_bar(picture);
    mesh.add_rectangle(bar.top_left, bar.size, kManaBarBackColor);
    // Whole pixels keep the end of the bar sharp, and a party with any mana left keeps one.
    const float fraction = magic.mana_fraction();
    const float filled = fraction > 0.0f ? std::max(1.0f, std::round(bar.size.x * fraction)) : 0.0f;
    if (filled > 0.0f) {
        mesh.add_rectangle(bar.top_left, {filled, bar.size.y}, kManaBarColor);
    }
}

}  // namespace manifold
