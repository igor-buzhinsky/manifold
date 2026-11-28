#pragma once

#include <memory>

#include "ui/menu_screen.hpp"

namespace manifold {

/// Which saves there are to load, see SaveKind, for the menus that offer to load them.
struct SavesToLoad {
    bool progress = false;     ///< Whether there is `save.json`.
    bool level_start = false;  ///< Whether there is `save_level_start.json`.
};

/// The menu shown when the game starts and there is a save: on black, with Continue (UiCommand::kLoad), Restart last
/// level (UiCommand::kLoadLevelStart), New game (UiCommand::kNewGame) and Exit game (UiCommand::kExit). Escape
/// doesn't leave it.
///
/// \param saves The saves there are. Continue and Restart last level are disabled without theirs, e.g. when the
///              `menu` command opens the menu without a save.
std::unique_ptr<MenuScreen> make_start_menu(SavesToLoad saves);

/// The game menu that Escape opens, over the dimmed game: Resume, New game, Load game (UiCommand::kLoad), Restart last
/// level (UiCommand::kLoadLevelStart) and Exit game. Escape resumes.
///
/// \param saves The saves there are. Load game and Restart last level are disabled without theirs.
std::unique_ptr<MenuScreen> make_game_menu(SavesToLoad saves);

/// The menu shown over the dimmed game once all characters of the party are dead, see Game::defeated(): Game over, with
/// Restart last level (UiCommand::kLoadLevelStart), New game and Exit game. Escape doesn't leave it.
///
/// \param saves The saves there are. Restart last level is disabled without the save of the level's start.
std::unique_ptr<MenuScreen> make_game_over_menu(SavesToLoad saves);

}  // namespace manifold
