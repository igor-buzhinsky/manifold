#include "ui/game_menus.hpp"

#include <memory>
#include <optional>

#include "ui/menu_screen.hpp"
#include "ui/screen.hpp"

namespace manifold {

std::unique_ptr<MenuScreen> make_start_menu(bool can_load) {
    return std::make_unique<MenuScreen>(MenuConfig{
        .title = "A saved game was found",
        .items = {{.label = "Continue", .command = UiCommand::kLoad, .enabled = can_load},
                  {.label = "New game", .command = UiCommand::kNewGame},
                  {.label = "Exit game", .command = UiCommand::kExit}},
        .cancel = std::nullopt,
        .hides_game = true,
    });
}

std::unique_ptr<MenuScreen> make_game_menu(bool can_load) {
    return std::make_unique<MenuScreen>(MenuConfig{
        .title = "Paused",
        .items = {{.label = "Resume", .command = UiCommand::kResume},
                  {.label = "New game", .command = UiCommand::kNewGame},
                  {.label = "Load game", .command = UiCommand::kLoad, .enabled = can_load},
                  {.label = "Exit game", .command = UiCommand::kExit}},
        .cancel = UiCommand::kResume,
        .hides_game = false,
    });
}

}  // namespace manifold
