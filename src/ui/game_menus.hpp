#pragma once

#include <memory>

#include "ui/menu_screen.hpp"

namespace manifold {

/// The menu shown when the game starts and there is a save: on black, with Continue (UiCommand::kLoad) and New game
/// (UiCommand::kNewGame). Escape doesn't leave it.
std::unique_ptr<MenuScreen> make_start_menu();

/// The game menu that Escape opens, over the dimmed game: Resume, New game, Load game and Exit game. Escape resumes.
///
/// \param can_load Whether there is a save to load. If not, Load game is disabled.
std::unique_ptr<MenuScreen> make_game_menu(bool can_load);

}  // namespace manifold
