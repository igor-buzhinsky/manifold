# Data

Files that define the game rather than how it looks and sounds, so that they can be changed without building the game
again. The game reads them from the `data` directory next to its executable in a copy of the game (see `cmake
--install` in [README.md](../README.md)), otherwise from this one. Later this directory will also hold level settings
and other constants of the game.

- `items.json`: all kinds of items, with their names, values, weights and chances, see the Items section of
  [DEBUGGING.md](../DEBUGGING.md#items). Their icons are listed under `ITEM_ICONS` of `assets/asset_mapping.json`.
