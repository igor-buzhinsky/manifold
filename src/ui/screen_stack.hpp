#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// The screens of the interface that are open, e.g. a menu, and later a dialog opened from it.
///
/// Only the screen on top gets events, and all are drawn, bottom first. The game is paused while any screen is
/// open, which the main loop sees from empty().
/// What one screen draws: its shapes, and the icons of items over them.
struct ScreenLayer {
    OverlayMesh mesh;
    std::vector<IconPlacement> icons;
};

class ScreenStack {
public:
    /// Opens `screen` on top of the others.
    void push(std::unique_ptr<Screen> screen);
    /// Closes the screen on top, if any.
    void pop();
    /// Closes all screens.
    void clear() { screens_.clear(); }

    bool empty() const { return screens_.empty(); }
    std::size_t size() const { return screens_.size(); }

    /// Passes an event to the screen on top, laid out over a picture of `picture` size.
    ///
    /// \return What the screen asks for, or nothing, also when no screen is open.
    std::optional<UiCommand> handle(const UiEvent& event, const PixelSize& picture);

    /// All screens laid out over a picture of `picture` size, bottom first, so that the top one is drawn last.
    OverlayMesh build_mesh(const PixelSize& picture) const;

    /// What each screen draws, laid out over a picture of `picture` size, bottom first, see Screen::add_to() and
    /// Screen::add_icons(). Each layer, the shapes and then the icons, is drawn before the next.
    std::vector<ScreenLayer> build_layers(const PixelSize& picture) const;

    /// Closes the screens that show something of the level, see Screen::shows_level(), e.g. when another level starts.
    /// The others stay in their order.
    void close_level_screens();

    /// Whether one of the screens hides the game, see Screen::hides_game(), so it needn't be drawn.
    bool hides_game() const;

private:
    std::vector<std::unique_ptr<Screen>> screens_;
};

}  // namespace manifold
