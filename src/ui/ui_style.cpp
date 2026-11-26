#include "ui/ui_style.hpp"

#include "render/viewport.hpp"

namespace manifold {

namespace {

/// Picture height per interface pixel, see ui_scale().
constexpr float kHeightPerScale = 300.0f;

}  // namespace

float ui_scale(const PixelSize& picture) { return static_cast<float>(picture.height) / kHeightPerScale; }

}  // namespace manifold
