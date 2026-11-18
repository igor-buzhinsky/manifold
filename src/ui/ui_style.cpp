#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

#include "render/viewport.hpp"

namespace manifold {

namespace {

/// Picture height per interface pixel, see ui_scale().
constexpr float kHeightPerScale = 300.0f;

}  // namespace

float ui_scale(const PixelSize& picture) {
    return std::max(1.0f, std::round(static_cast<float>(picture.height) / kHeightPerScale));
}

}  // namespace manifold
