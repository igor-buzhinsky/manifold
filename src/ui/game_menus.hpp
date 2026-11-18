#pragma once

#include <memory>

#include "ui/menu_screen.hpp"

namespace manifold {

/// The menu shown when the game starts and there is a save: on black, with Continue (UiCommand::kLoad), New game
/// (UiCommand::kNewGame) and Exit game (UiCommand::kExit). Escape doesn't leave it.
///
/// \param can_load Whether there is a save to load. If not, Continue is disabled, e.g. when the `menu` command opens
///                 the menu without a save.
std::unique_ptr<MenuScreen> make_start_menu(bool can_load);

/// The game menu that Escape opens, over the dimmed game: Resume, New game, Load game and Exit game. Escape resumes.
///
/// \param can_load Whether there is a save to load. If not, Load game is disabled.
std::unique_ptr<MenuScreen> make_game_menu(bool can_load);

}  // namespace manifold
