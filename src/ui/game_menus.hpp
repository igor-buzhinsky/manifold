#pragma once

#include <memory>

#include "ui/menu_screen.hpp"

namespace manifold {

/// Which saves there are to load, see SaveKind, for the menus that offer to load them.
struct SavesToLoad {
    bool progress = false;     ///< Whether there is `save.json`.
    bool level_start = false;  ///< Whether there is `save_level_start.json`.
};

/// The menu shown when the game starts and there is a save: on black, with Continue (UiCommand::kLoad), Continue from
/// level start (UiCommand::kLoadLevelStart), New game (UiCommand::kNewGame) and Exit game (UiCommand::kExit). Escape
/// doesn't leave it.
///
/// \param saves The saves there are. Continue and Continue from level start are disabled without theirs, e.g. when the
///              `menu` command opens the menu without a save.
std::unique_ptr<MenuScreen> make_start_menu(SavesToLoad saves);

/// The game menu that Escape opens, over the dimmed game: Resume, New game, Load game (UiCommand::kLoad), Restart
/// level (UiCommand::kLoadLevelStart) and Exit game. Escape resumes.
///
/// \param saves The saves there are. Load game and Restart level are disabled without theirs.
std::unique_ptr<MenuScreen> make_game_menu(SavesToLoad saves);

}  // namespace manifold
