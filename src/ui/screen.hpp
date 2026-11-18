#pragma once

#include <cstdint>
#include <optional>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// What a screen of the interface asks the game to do, e.g. when a menu item is chosen.
enum class UiCommand : std::uint8_t {
    kResume,   ///< Close the screen and go back to the game.
    kNewGame,  ///< Start a new game, which replaces the save.
    kLoad,     ///< Load the saved game.
    kExit,     ///< Quit the game.
};

/// A screen of the game's interface, such as a menu, shown over the game or instead of it, see ScreenStack.
///
/// A screen keeps its state, such as which control has the focus, but no layout: it lays itself out anew for each
/// event and each drawing, for the size of the picture that is passed, so that it always matches what is shown.
/// Later screens, such as a party screen with tabs, are made of the same controls, e.g. ButtonList.
class Screen {
public:
    Screen() = default;
    virtual ~Screen() = default;
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    Screen(Screen&&) = delete;
    Screen& operator=(Screen&&) = delete;

    /// Handles an event, with the screen laid out over a picture of `picture` size.
    ///
    /// \return What the event asks the game to do, if anything.
    virtual std::optional<UiCommand> handle(const UiEvent& event, const PixelSize& picture) = 0;

    /// Adds the screen, laid out over a picture of `picture` size, to `mesh`.
    virtual void add_to(OverlayMesh& mesh, const PixelSize& picture) const = 0;

    /// Whether the game is hidden behind the screen, so it needn't be drawn. Otherwise the game is seen behind it.
    virtual bool hides_game() const = 0;
};

}  // namespace manifold
