#pragma once

#include <cstdint>
#include <variant>

#include <glm/vec2.hpp>

namespace manifold {

// Input for the game's interface (menus and later other screens), independent of the input device. The main loop
// turns keys and mouse events into these.

/// A key that moves through a screen or chooses on it.
enum class NavKey : std::uint8_t {
    kUp,       ///< Moves the focus to the previous control (Up).
    kDown,     ///< Moves the focus to the next control (Down).
    kConfirm,  ///< Activates the control with the focus (Enter or Space).
    kCancel,   ///< Leaves the screen, if it can be left (Escape).
};

/// What the pointer (the mouse) did.
enum class PointerAction : std::uint8_t {
    kMove,     ///< It moved.
    kPress,    ///< The main button (the left mouse button) went down.
    kRelease,  ///< The main button went up.
};

/// An action of the pointer.
struct PointerEvent {
    PointerAction action = PointerAction::kMove;
    glm::vec2 position{0.0f};  ///< In pixels from the top-left corner of the picture, y down, as in OverlayMesh.
};

/// An input event for a screen of the interface.
using UiEvent = std::variant<NavKey, PointerEvent>;

}  // namespace manifold
