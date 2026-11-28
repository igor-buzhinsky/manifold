# Data

Files that define the game rather than how it looks and sounds, so that they can be changed without building the game
again: restarting it is enough. The game reads them from the `data` directory next to its executable in a copy of the
game (see `cmake --install` in [README.md](../README.md)), otherwise from this one. The movement simulation
(`manifold_sim`) reads the ones in this directory; the unit tests use the defaults in code instead, so tuning a value
here doesn't break them, except a test that these files load without problems or warnings.

- `items.json`: all kinds of items, with their names, values, weights and chances, see the Items section of
  [DEBUGGING.md](../DEBUGGING.md#items). Their icons are listed under `ITEM_ICONS` of `assets/asset_mapping.json`. An
  item with a wrong field is left out, with a warning.
- `spells.json`: each spell by its key (`candlelight`, `levitate`), which saves, the `cast` command and
  `SPELL_SOUNDS` of `assets/asset_mapping.json` use, with its `name` as the screen shows it, its `cost` in mana and
  its `duration` in seconds of game time. What a spell does is in code, so a spell can't be added here.
- `level_layouts.json`: the settings of the two layouts of levels, `rooms` and `debug`. Each has a `probability`, how
  often it is chosen for a level relative to the other (0 for the debug layout, which is for testing and is only used
  when `--layout debug` asks for it, see [README.md](../README.md)), and the fields of `RoomLayoutConfig` and
  `DebugLayoutConfig` (`src/level/room_layout.hpp`, `src/level/debug_layout.hpp`) by their names, which those structs
  explain, such as the size of the grid, the number of layers (`layer_count`), the number of holes and ramps between
  layers, and the chances of torches, doors and chests.
- `constants.json`: the other constants, in groups:
  - `player`: the `radius` and `height` of the player's body in metres, `walk_speed` and `run_speed` in metres per
    second, `turn_speed` and `look_speed` (of the keys) in degrees per second, `jump_height` in metres, and
    `max_snap_slope`, the steepest slope (height per horizontal distance) that the player sticks to when walking down.
  - `party`: the number of characters of a new party (`size`), their `max_hp`, and the party's `max_mana` and
    `mana_regeneration` per second. A loaded party keeps its own hit points and mana, and their maximums.
  - `fall_damage`: how landing from a fall hurts each character (`FallDamageConfig` in
    `src/party/fall_damage.hpp`): the fastest landing that doesn't hurt (`safe_speed`, in metres per second), the
    landing speed that takes 100 hit points on average (`full_damage_speed`, faster than `safe_speed`), and the
    `spread` of each character's damage around that of the speed, as a part of it from 0 to 1 (0.3: up to 30% more or
    less). The damage grows with the square of the speed above `safe_speed`, and doesn't depend on `max_hp`.
  - `level_objects`: the weights of the flames of torches (`torch_flames`: `orange`, `blue`, `green`, which needn't add
    up to 1), the `golden_chest_chance`, and how a `wooden_chest` and a `golden_chest` are filled: how many items are
    drawn (`min_draws` to `max_draws`), which values the items may have (`min_item_value` to `max_item_value`), and the
    most that they may be worth together (`max_total_value`), see the Items section of
    [DEBUGGING.md](../DEBUGGING.md#items).
  - `lighting`: the `candlelight` (`color` as linear RGB, `intensity` at 1 m, `range` in metres), the `intensity` and
    `range` of each `torch` and of the `exit`, the `fade_distance` over which lights fade out before they are left out,
    and the `level_ambient_luminance` of the ambient light of the levels that have it. The debug overlay changes the
    lights while the game runs, and its Reset lights button puts back these values.

Unlike `items.json`, `spells.json`, `level_layouts.json` and `constants.json` must be complete and right: each field
must be there, with a value of the right type and range, and an unknown field, such as a misspelt one, is an error too.
Some fields must fit together, such as the walk, which can't be faster than the run, also across files: the player must
be lower than a layer of rooms, and the party's `max_mana` must pay for every spell. Counts, attempts and the size of
the debug level have upper limits, so that a typo with too many zeros can't take all memory or time. The game also
generates a level of each layout at the start, from one seed, since some settings only fail together, such as tiles too
narrow for the player. If anything is wrong, the game doesn't start, and the log lists every problem with its file and
field, e.g. `data/spells.json: levitate.duration is -1, not a positive number`.

A save keeps the party's mana and spells but not the level, which is generated again from its seed. So a saved game
gives another level once `level_layouts.json` or the torch flames, the golden chest chance or the loot of chests in
`constants.json` change, as it does when items or assets change. A spell in effect lasts at most its new `duration`
after loading.
