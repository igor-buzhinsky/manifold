#pragma once

#include <SDL3/SDL_events.h>

#include "console/game_commands.hpp"
#include "game.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "util/frame_times.hpp"

namespace manifold {

/// The debug overlay: a Dear ImGui window that tunes the lights and shows how the game runs.
///
/// It needs the window's OpenGL context to be current for its whole lifetime. There must be only one at a
/// time, because ImGui keeps global state.
class DebugOverlay {
public:
    /// Sets up ImGui for the window, scaled to the window's display.
    ///
    /// \param window The window to draw into. It must outlive the overlay.
    /// \throws std::runtime_error if the ImGui backends can't be set up.
    explicit DebugOverlay(const Window& window);
    ~DebugOverlay();
    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;
    DebugOverlay(DebugOverlay&&) = delete;
    DebugOverlay& operator=(DebugOverlay&&) = delete;

    /// Lets the overlay get events, or not, e.g. while it is hidden or the console window has the keys.
    ///
    /// When input is turned on, the keys and mouse buttons that ImGui remembers count as released, because
    /// their events were missed. When it is turned off, a control that is in use, such as a value being
    /// typed, is let go, and text input is turned off if ImGui turned it on for that.
    void set_input(bool on);

    /// Passes an event to ImGui, if input is on.
    ///
    /// \return True if the game should ignore the event, because ImGui uses it: a key or typed text while
    ///         ImGui has the keyboard, see wants_keyboard().
    bool handle_event(const SDL_Event& event);

    /// Whether ImGui uses the keyboard, so the keys shouldn't move the player: while a value is typed into a
    /// slider, and also while a control is held with the mouse. Always false while input is off.
    bool wants_keyboard() const;

    /// Lays out the overlay and draws it over the bound framebuffer, which must be the window's.
    ///
    /// \param game   What to show. The overlay can turn noclip on or off.
    /// \param view   How the game is shown, which the overlay changes.
    /// \param stats  What the renderer did in this frame.
    /// \param frames Durations of the last frames.
    void draw(Game& game, ViewSettings& view, const RenderStats& stats, const FrameTimes& frames);

private:
    SDL_Window* window_;
    float scale_ = 1.0f;  ///< Size of the overlay relative to ImGui's default, from the display scale.
    bool input_ = false;
};

}  // namespace manifold
