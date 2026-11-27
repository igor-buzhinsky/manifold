# Manifold — Game Manual

## Controls

| Key | Action |
|---|---|
| W / S | Walk forward / backward, at 2.5 m/s (a brisk walk) |
| Shift (left or right) + W | Run forward, at 4.5 m/s. Only the forward part of the movement is faster: walking backward and stepping aside stay at walking speed. Running into a wall, a block or a door at more than 30° from along it slows down to walking speed, also when sliding along it; at a shallower angle, the party slides along it at running speed. Running works on the ground and with noclip, but not while Levitate is in effect. In the air, the party keeps the speed it had when it left the ground, so a running jump goes farther |
| A / D | Step left / right |
| Left / Right arrow | Turn left / right |
| Up / Down arrow | Look up / down (at most 40° each way) |
| Page Up | Jump (0.5 m high under Earth gravity, higher where gravity is weaker). Press again for the next jump. In the air, after a jump or a step off a ledge, the party keeps the horizontal speed and direction it had when it left the ground: the keys don't change them, and turning only turns the view. A wall stops only the part of the movement into it. With noclip or Levitate: fly up |
| Page Down | With noclip or Levitate: fly down |
| E | Activate what the center of the view points at, up to 2 m away: open a closed door, open or close a door by aiming at one of its dark frames, or open a chest to see what it holds (see Inventory below). Press again for the next activation |
| Mouse | With mouse look on: turn left / right and look up / down (at most 40° each way, as with the keys), see Mouse look below |
| Left mouse button | Activate what the clicked point of the view shows, as E does when aiming at it: from the eyes, up to 2 m away. The point can be anywhere in the view, also beyond the 40° that the view can be turned up or down. Each click activates once. Clicks on the portraits of the party, or between them, on the spells in effect or between them, and on the bar of mana do nothing, nor do clicks while the console is open, while the debug overlay has the keyboard, and while `camera` shows the view from elsewhere. With mouse look on, a click activates what the center of the view points at, as E does |
| I | Open or close the party's inventory, see Inventory below |
| M | Turn mouse look on or off, see Mouse look below |
| C | Cast Candlelight, see Spells below |
| L | Cast Levitate, see Spells below |
| F9 | Load the saved game: start the saved level again from its start, see Saving below |
| `/~ (left of 1) | Open or close the console, see [DEBUGGING.md](DEBUGGING.md#console) |
| F3 | Show or hide the debug overlay, see [DEBUGGING.md](DEBUGGING.md#debug-overlay) |
| Escape | Close the console if it is open, otherwise open the game menu, or close it to resume, see Menus below |
| Ctrl+Q | Quit at once, also from the console or a menu. Closing the window does the same |

## Menus

Escape opens the game menu over the darkened game, which is paused while the menu is open: torches, doors and the
portal stand still. It has these items:

- **Resume**: close the menu and go on playing, as Escape does.
- **New game**: start a new game with a random level, which replaces the save. A layout given with `--layout` is used
  for its first level.
- **Load game**: load the save, as F9 does. It is greyed out if there is no save.
- **Exit game**: quit the game.

When the game starts and there is a save, it asks on a black screen whether to **Continue** the saved game, start
a **New game**, which replaces the save once it is chosen, or **Exit game**. Escape doesn't leave this menu. Closing
it otherwise, with the `menu` command, also starts the new game and replaces the save. A save that can't be read,
e.g. one of an older version of the game, isn't offered: a new game starts, as without a save, and replaces it.

Menus are used with the keys or the mouse:

| Key | Action |
|---|---|
| Up / Down | Move to the previous / next item, skipping greyed out ones |
| Enter or Space | Choose the highlighted item |
| Mouse | Pointing at an item highlights it, and clicking it with the left button chooses it |

The console can be opened over a menu; while it is open, the menu gets no keys or clicks. The `load` command closes
the menus, as Load game does. The debug overlay (F3) can't be used while a menu is open, and isn't shown over the
start menu.

## Screen

A thin white crosshair with a black outline marks the center of the view, which E activates. The mouse pointer
shows what a click activates; with mouse look on, it is hidden, and a click activates at the crosshair. The
`crosshair` command hides or shows the crosshair.

The spells in effect are listed in the top-right corner, each name in a white box, in the order they were first cast.
A box blinks in the last 10 seconds of its spell. The blue bar in the bottom-right corner shows the party's mana.
Clicks on them, or between the boxes, don't activate what is behind.

## Mouse look

With mouse look on, the mouse turns the view, as the arrow keys do: moving it left or right turns, and moving it up or
down looks up or down, at most 40° each way. The mouse pointer is hidden and kept in the window, and a left click
activates what the crosshair points at, as E does. The arrow keys still turn the view too. The click that gives the
game's window the focus back doesn't activate.

M turns mouse look on or off while playing. It starts as the `mouse_look` setting says (off by default, see Settings
below), and M doesn't change the setting, so the next start is as the setting says again. Runs with `--exec` or
`--screenshot` (see the Console section of [DEBUGGING.md](DEBUGGING.md#console)) start with mouse look off, so they
don't take the mouse; M still turns it on.

While a menu, the inventory, the console or the debug overlay (F3) is open, mouse look pauses: the pointer comes back, to be used
with them, and the mouse doesn't turn the view. Mouse look goes on again once they are closed. It also lets the
pointer go while the game's window doesn't have the focus.

## Settings

The game reads its settings from two JSON files, if they exist:

1. The game's own `settings.json`, next to the executable in a copy of the game, otherwise in the source tree.
2. The player's `settings.json`, in the same directory as the save (see Saving below): `$XDG_DATA_HOME/manifold/` on
   Linux, by default `~/.local/share/manifold/`. A setting given here replaces the game's own; settings it doesn't
   give stay as the game's file has them.

```json
{
  "fullscreen": true,
  "mouse_look": false,
  "mouse_sensitivity": 0.1
}
```

- `fullscreen`: `true` fills the screen at its current resolution, `false` opens a window of 1600 x 900. The picture
  keeps its aspect ratio of 16:9 and is scaled to the screen, with black bars above and below, or on the sides,
  where the screen's aspect ratio differs. The game's own file sets `true`; without any file, the game runs in a
  window. Runs with `--exec` or `--screenshot` (see the Console section of [DEBUGGING.md](DEBUGGING.md#console))
  always open a window.
- `mouse_look`: `true` starts the game with mouse look on, `false` with it off, as without any file. M turns it on
  or off while playing, see Mouse look above.
- `mouse_sensitivity`: how far mouse look turns the view, in degrees per unit of mouse motion, a number greater than
  0. By default 0.1, so moving the mouse by 900 units turns the view by 90°. The units are those of the mouse, often
  its counts, so the same setting turns more with a mouse of a higher resolution.

Unknown settings are ignored with a warning in the log. A file that isn't valid JSON or has a setting of a wrong
type or value is skipped as a whole, with an error in the log. The settings are read only when the game starts.

## Party

The player leads a party of three characters, who move together. Their portraits are in the bottom-left corner of the
screen, with a thin red bar below each that shows how many of the character's hit points are left. Each new game
chooses the portraits at random, all different, and each character's gender goes with the portrait. Nothing lowers the
hit points yet, except the `hp` cheat in the console. Clicking a portrait, or between them, does nothing yet, and doesn't activate what is behind it. If the game has fewer
portraits than characters, the last characters have an empty dark place instead.

## Spells

The numbers of this section and of the chests below are those of the game's data files, which can change them, see
[data/README.md](data/README.md).

The party shares its mana: 100 at the start of a game, which is also the most it can have. It regains 1 mana per second,
also while spells are in effect, but not while the game is paused. Each spell costs mana when it is cast and lasts 60
seconds of game time. Casting a spell that is in effect starts its 60 seconds again; the time that was left is not
added. If the party has less mana than a spell costs, nothing happens.

| Spell | Key | Cost | Effect |
|---|---|---|---|
| Candlelight | C | 5 | A warm light at the party's eyes. Without it the party has no light of its own, and sees only by the torches, the portal and the level's ambient light, if any |
| Levitate | L | 10 | No gravity: Page Up and Page Down fly up and down, and the party floats where it is. Walls, floors, ceilings and closed doors still stop it, unlike noclip. It makes no footsteps. When the spell ends in the air, the party falls |

A new game starts without Candlelight. So that the party can see where it is, the first level always has a torch on
one of the walls nearest to where the party starts, also if the room has other torches, unless every wall of the room
already has one. The debug layout (`--layout debug`) has no rooms, and gets no such torch.

Mana and the spells in effect stay as they are in the next level. The `cast` and `mana` commands of the console cast
spells and set the mana, see the Console section of [DEBUGGING.md](DEBUGGING.md#console).

## Inventory

The party shares one inventory, which has no limit of weight yet. I opens it over the darkened game, which is paused
while it is open, as with a menu: a grid of the items, each in a cell of its own, and their total weight. Weights are in
kilograms, shown without the unit. Items of the same kind aren't stacked yet, so each takes a cell. For now, the only
items are gems: small and large diamonds, rubies, emeralds, sapphires and amethysts.

Activating a chest opens its items in a grid on the left, with the party's on the right, each narrower. What a chest
holds is chosen when the level is generated: a wooden chest holds at most three items, usually one or two, and a golden
one at most five, usually two to four, and more valuable ones. Items can be moved between the two grids, and they stay
where they were put while the party is in the level. A level that starts again, e.g. after loading, has its chests full
again.

| Key | Action |
|---|---|
| Left mouse button | On an item: select it, which shows its name and weight at the bottom. On an empty cell: move the selected item to the end of that grid's items, the party's or the chest's, and the items after its old place move up |
| Arrow keys | Move the selection, from one grid to the other at their edges. Without a selection, select the first item |
| Double click | With a chest open: move the item from the chest to the end of the party's items, or from the party's into the chest. Without a chest, it only selects the item |
| Enter or Space | With a chest open: move the selected item to the end of the other grid, and select the item that comes into its place |
| T, or the Take all button | Move all items of the chest to the end of the party's, in their order |
| Mouse wheel | Scroll the grid under the pointer. Each grid has an empty cell after its items, so that an item can always be put there |
| Escape or I | Close the inventory |

## Levels

Each level has an exit: a spinning blue portal cube floating above the floor, whose pulsing light shows
the way to it. In room levels it is on another layer than the start. Touching it, also with noclip, starts
the next level, with the sound of the portal. Each level's portal has its own sound, picked from the level seed.

Walking on the ground makes a footstep about every metre, and running about every 1.5 metres. Landing from a fall of
more than about 0.2 m, such as a jump onto the same floor, makes a footstep too. The steps sound like what
the level's texture shows: grass, wood, metal or stone (also bricks, tiles, plaster and dirt).

In room levels, some openings between rooms (10% to 100% of them, random for each level) have doors: two
halves between two dark frames. All doors of a level are wooden or all are metal, each with a chance of 50%.
A closed door blocks the way. E on the door opens it: the halves slide into the walls within a second, and
only then can the player pass. To close an open door, aim at one of its frames and press E. Clicking the door or
a frame does the same. A door that is moving ignores E and clicks, and a door doesn't close while the player stands
in it.
A door makes a sound when it starts to open and when it starts to close, also with the `door` command. Each door
has its own two sounds, picked when the level is generated.

Small chests stand at walls. In room levels, each room at least 2 tiles wide and deep, except the one where the player
starts, has a chest with a chance of 20%. In the debug layout, chests stand below every third torch, unless something is in the way. A chest is wooden,
or golden with a chance of 20%. Chests don't block the way. Activating one, by looking down at it with E or by clicking
it, plays the sound of opening it and shows what it holds next to the party's items, see Inventory above. A golden
chest holds more items than a wooden one on average, and more valuable ones.

## Saving

The game saves itself when it starts and when the player reaches a new level. There is only one save, of the
level the player is in, so a level that is left can't be returned to, and there are no saves made by hand. F9 loads
the save: the saved level starts again from its start. When the game starts and there is a save, it asks
whether to continue it or to start a new game, which replaces the save, see Menus above. New game in the game menu
also replaces it.

The save is `save.json` in the game's data directory: `$XDG_DATA_HOME/manifold/` on Linux, by default
`~/.local/share/manifold/`. It holds the seed of the level, how many levels came before it, the game time at its start, the
party as it was then (each character's portrait, as its key and index in the asset mapping, see [DEBUGGING.md](DEBUGGING.md#assets), gender and hit points, the party's mana and the spells in effect
with the time left of each, and the keys of the items it carried, see [DEBUGGING.md](DEBUGGING.md#items)), and the layout if it was given with
`--layout`. Levels are generated again from their seed, so a save of an older
version of the game may give another level, or be refused if the save format changed.

A run with `--exec` or `--screenshot` (see the Console section of [DEBUGGING.md](DEBUGGING.md#console)) never saves,
not even the levels reached after its commands, so that test runs don't replace the player's save. They can still load it. They also don't ask whether to continue the save,
but start a new game.
