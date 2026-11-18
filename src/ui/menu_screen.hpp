#pragma once

#include <optional>
#include <string>
#include <vector>

#include "render/viewport.hpp"
#include "ui/button_list.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// An item of a MenuScreen.
struct MenuItem {
    std::string label;
    UiCommand command = UiCommand::kResume;  ///< What choosing the item asks for.
    bool enabled = true;                     ///< A disabled item is greyed out and can't be chosen.
};

/// What a MenuScreen shows and does.
struct MenuConfig {
    std::string title;
    std::vector<MenuItem> items;  ///< Top to bottom.
    /// What NavKey::kCancel (Escape) asks for, or nothing if the menu can't be left that way.
    std::optional<UiCommand> cancel;
    /// Whether the menu is on black instead of over the dimmed game, see Screen::hides_game().
    bool hides_game = false;
};

/// A menu: a dark grey panel in the middle of the picture with a title and a column of buttons, one for each item.
class MenuScreen : public Screen {
public:
    /// Sizes at scale 1, in interface pixels, see Placement::scale.
    static constexpr float kPadding = 10.0f;  ///< Between the edge of the panel and what is in it.
    static constexpr float kTitleGap = 8.0f;  ///< Between the title and the buttons.
    static constexpr float kMinButtonWidth = 120.0f;
    static constexpr float kLabelPadding = 12.0f;  ///< Least space left and right of the longest label in a button.

    explicit MenuScreen(MenuConfig config);

    /// Up, Down, Enter and the mouse go to the buttons; choosing one gives its MenuItem::command. Escape gives
    /// MenuConfig::cancel.
    std::optional<UiCommand> handle(const UiEvent& event, const PixelSize& picture) override;

    void add_to(OverlayMesh& mesh, const PixelSize& picture) const override;

    bool hides_game() const override { return hides_game_; }

    /// The rectangle of the panel over a picture of `picture` size.
    Rect panel(const PixelSize& picture) const;

    /// Where the buttons are over a picture of `picture` size.
    Placement buttons_placement(const PixelSize& picture) const;

    const ButtonList& buttons() const { return buttons_; }

private:
    std::string title_;
    std::vector<UiCommand> commands_;  ///< The command of each button.
    std::optional<UiCommand> cancel_;
    bool hides_game_;
    ButtonList buttons_;
};

}  // namespace manifold
