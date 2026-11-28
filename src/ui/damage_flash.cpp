#include "ui/damage_flash.hpp"

#include <algorithm>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

namespace {

/// The red of the flash, in sRGB, without its opacity.
constexpr glm::vec3 kDamageFlashColor{0.8f, 0.0f, 0.0f};

}  // namespace

float damage_flash_opacity(float elapsed, int damage) {
    if (damage <= 0 || elapsed < 0.0f || elapsed >= kDamageFlashDuration) {
        return 0.0f;
    }
    const float strength = std::min(static_cast<float>(damage) / kDamageFlashFullDamage, 1.0f);
    const float peak = kDamageFlashMinOpacity + ((kDamageFlashMaxOpacity - kDamageFlashMinOpacity) * strength);
    return peak * (1.0f - (elapsed / kDamageFlashDuration));
}

void add_damage_flash(OverlayMesh& mesh, const PixelSize& picture, float opacity) {
    if (opacity <= 0.0f) {
        return;
    }
    mesh.add_rectangle({0.0f, 0.0f}, {static_cast<float>(picture.width), static_cast<float>(picture.height)},
                       glm::vec4(kDamageFlashColor, std::min(opacity, 1.0f)));
}

}  // namespace manifold
