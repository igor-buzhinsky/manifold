#pragma once

#include <glm/vec4.hpp>

#include "render/viewport.hpp"

namespace manifold {

// Colors and sizes shared by the screens of the game's interface, so that they look alike. Colors are sRGB and
// opacity, each in [0, 1], as in OverlayMesh.

/// Background of panels, such as menus: dark grey, nearly opaque.
inline constexpr glm::vec4 kUiPanelColor{0.16f, 0.16f, 0.17f, 0.97f};
/// Lines around panels and the control with the focus.
inline constexpr glm::vec4 kUiBorderColor{0.5f, 0.5f, 0.55f, 1.0f};
/// Background of a button.
inline constexpr glm::vec4 kUiButtonColor{0.24f, 0.24f, 0.26f, 1.0f};
/// Background of the button with the focus, which the mouse is over or the keys moved to.
inline constexpr glm::vec4 kUiFocusColor{0.36f, 0.36f, 0.4f, 1.0f};
/// Background of a button that is held down with the mouse.
inline constexpr glm::vec4 kUiPressedColor{0.2f, 0.2f, 0.22f, 1.0f};
inline constexpr glm::vec4 kUiTextColor{0.9f, 0.9f, 0.9f, 1.0f};
/// Text of controls that can't be used now.
inline constexpr glm::vec4 kUiDisabledTextColor{0.48f, 0.48f, 0.5f, 1.0f};
/// Titles of panels.
inline constexpr glm::vec4 kUiTitleColor{1.0f, 0.85f, 0.55f, 1.0f};
/// Laid over the game behind a screen that leaves it visible, to set the screen apart from it.
inline constexpr glm::vec4 kUiDimColor{0.0f, 0.0f, 0.0f, 0.5f};
/// Behind a screen that hides the game.
inline constexpr glm::vec4 kUiBlackColor{0.0f, 0.0f, 0.0f, 1.0f};

/// Size of an interface pixel in pixels of a picture of `picture` size: its height divided by 300, so that the
/// interface is 300 interface pixels high and looks the same at every size, e.g. 1.5 at 450 pixels, 3 at 900 and 3.6 at
/// 1080. At heights that aren't multiples of 300, interface pixels aren't whole pixels of the picture, so the blocky
/// font and thin lines look a little uneven.
float ui_scale(const PixelSize& picture);

}  // namespace manifold
