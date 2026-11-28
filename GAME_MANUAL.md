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
| F9 | Load the saved game as it was at the last autosave or exit (`save.json`), see Saving below |
| `/~ (left of 1) | Open or close the console, see [DEBUGGING.md](DEBUGGING.md#console) |
| F3 | Show or hide the debug overlay, see [DEBUGGING.md](DEBUGGING.md#debug-overlay) |
| Escape | Close the console if it is open, otherwise open the game menu, or close it to resume, see Menus below |
| Ctrl+Q | Quit at once, also from the console or a menu. Closing the window does the same |

## Menus

Escape opens the game menu over the darkened game, which is paused while the menu is open: torches, doors and the
portal stand still. It has these items:

- **Resume**: close the menu and go on playing, as Escape does.
- **New game**: start a new game with a random level, which replaces both saves. A layout given with `--layout` is
  used for its first level.
- **Load game**: load the game as it was at the last autosave or exit, as F9 does. It is greyed out if there is no
  `save.json`.
- **Restart last level**: start the saved level again from its start (`save_level_start.json`). `save.json` is
  replaced only by the next autosave or on exit, so until then, Load game undoes it. It is greyed out if there is no
  such save.
- **Exit game**: quit the game, which saves it.

When the game starts and there is a save, it asks on a black screen whether to **Continue** the saved game where it
was left, to **Restart last level** from its start, to start a **New game**, which replaces both saves once it is chosen, or
to **Exit game**, which keeps them. Escape doesn't leave this menu. Closing it otherwise, with the `menu` command, also
starts the new game and replaces the saves. A save that can't be read, e.g. one of an older version of the game, isn't
offered: its item is greyed out, and if neither save can be read, a new game starts, as without a save, and replaces
them.

Menus are used with the keys or the mouse:

| Key | Action |
|---|---|
| Up / Down | Move to the previous / next item, skipping greyed out ones |
| Enter or Space | Choose the highlighted item |
| Mouse | Pointing at an item highlights it, and clicking it with the left button chooses it |

Once all characters of the party are dead (see Party below), the **Game over** menu pauses the darkened game. It offers
to **Restart last level** from its start, as the game menu does, to start a **New game**, or to **Exit game**. Escape
doesn't leave it. While the whole party is dead, neither save is written, so `save.json` keeps the game as it was at the
last autosave before, and Continue in the start menu returns there after Exit game.

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
  "mouse_sensitivity": 0.1,
  "autosave": true,
  "autosave_interval": 10
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
- `autosave`: `true` saves the game to `save.json` every `autosave_interval` while playing, `false` only when a
  level starts and when the game quits, see Saving below. By default `true`.
- `autosave_interval`: seconds of play between autosaves, a number greater than 0, by default 10. Time in a menu or
  another screen that pauses the game doesn't count.

Unknown settings are ignored with a warning in the log. A file that isn't valid JSON or has a setting of a wrong
type or value is skipped as a whole, with an error in the log. The settings are read only when the game starts.

## Party

The player leads a party of three characters, who move together. Their portraits are in the bottom-left corner of the
screen, with a thin red bar below each that shows how many of the character's hit points are left. Each new game
chooses the portraits at random, all different, and each character's gender goes with the portrait. Clicking a
portrait, or between them, does nothing yet, and doesn't activate what is behind it. If the game has fewer portraits
than characters, the last characters have an empty dark place instead.

A character has 100 hit points, those of an ordinary person, and only falls take them away so far. Landing at more
than 8.5 m/s, after a fall of more than about 3.7 m, hurts, so dropping from one layer of rooms to the next (3.3 m)
doesn't: the faster the landing, the more, in proportion to its energy beyond that speed. A fall of 6.6 m takes about
23 hit points, one of 10 m about 50, and one of 16.5 m about 100. Each character gets its own share, up to 30% more or less
than that, so a fall may kill one and spare another. Only the vertical speed counts, so a running jump hurts no more
than a standing one (though a jump lands faster than a step off the same ledge), and weaker gravity makes the same
fall gentler. Flying down with Levitate doesn't hurt,
but a fall after it ends in the air does. The screen turns red for a moment when a landing hurts, the more the worse
the damage.

A character without hit points left is dead, and its portrait turns gray. Once all characters are dead, the game is
over, see the Game over menu above.

## Spells

The numbers of this section and of the chests below are those of the game's data files, which can change them, see
[data/README.md](data/README.md).

The party shares its mana: 100 at the start of a game, which is also the most it can have. It regains 1 mana per second,
also while spells are in effect, but not while the game is paused. Each spell costs mana when it is cast and lasts 60
seconds of game time. Casting a spell that is in effect starts its 60 seconds again; the time that was left is not
added. Casting a spell plays its sound. If the party has less mana than a spell costs, nothing happens, without a
sound.

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
while it is open, as with a menu: a grid of the items and their total weight. Weights are in kilograms, shown without the
unit. Items of the same kind are stacked: they share a cell, which shows their number in orange at its bottom right if
there are more than one (above 9999 shortened, e.g. 12k for 12345; the line at the bottom has the exact number). For
now, the only items are gems: small and large diamonds, rubies, emeralds, sapphires and amethysts.

Activating a chest opens its items in a grid on the left, with the party's on the right, each narrower. What a chest
holds is chosen when the level is generated: a wooden chest holds at most three items, usually one or two, and a golden
one at most five, usually two to four, and more valuable ones. Items can be moved between the two grids, and they stay
where they were put while the party is in the level. A level that starts again, e.g. after loading, has its chests full
again. Items moved to the other grid join the stack of their kind there, or start a new stack at its end.

Moving a stack of more than one item to the other grid, in any of the ways below, first asks how many of them to move,
all by default: Left and Right, the - and + buttons or the mouse wheel change the number by one, Up and Down by ten,
and clicking or dragging the slider chooses it. Enter or Move moves them, Escape, Cancel or a click outside the dialog
moves nothing. The rest of the stack stays where it was. Stacks can't be split within one grid, so the party's
inventory alone, without a chest, never asks. Take all moves everything without asking.

| Key | Action |
|---|---|
| Left mouse button | On a stack: select it, which shows its name, number of items and weight at the bottom. On an empty cell: move the selected stack to that grid, the party's or the chest's; in its own grid it goes to the end, and the stacks after its old place move up |
| Arrow keys | Move the selection, from one grid to the other at their edges. Without a selection, select the first stack |
| Double click | With a chest open: move the stack from the chest to the party's items, or from the party's into the chest. Without a chest, it only selects the stack |
| Enter or Space | With a chest open: move the selected stack to the other grid, and select the stack that comes into its place, or keep the rest selected if only some of its items moved |
| T, or the Take all button | Move all items of the chest to the party's, in their order |
| Mouse wheel | Scroll the grid under the pointer. Each grid has an empty cell after its stacks, so that a stack can always be put there |
| Escape or I | Close the inventory. While the dialog that asks how many items to move is open, Escape only closes the dialog |

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

The game keeps two saves, both in the game's data directory: `$XDG_DATA_HOME/manifold/` on Linux, by default
`~/.local/share/manifold/`.

- `save.json` holds the game as it was when it was last saved: when a level starts, every 10 seconds of play (see
  `autosave` and `autosave_interval` in Settings above) and when the game quits, by Exit game, Ctrl+Q or closing the
  window. F9, Load game and Continue load it.
- `save_level_start.json` holds the start of the level the player is in, and is written only when a level starts.
  Restart last level, in each menu that has it, loads it. It doesn't replace `save.json` at once: the next autosave
  does, one autosave interval of play later, or quitting the game does, and until then, F9 or Load game goes back to
  where the player was before. With `autosave` off, `save.json` stays until the game quits or the next level starts.

A level that is left can't be returned to, and there are no saves made by hand. When the game starts and there is a
save, it asks which one to continue or whether to start a new game, which replaces both, see Menus above. New game in
the game menu also replaces them. A game that stops without quitting, e.g. when it crashes, loses at most the play
since the last autosave. Each save is written to a temporary file next to it first, which then replaces it, so a game
that stops while saving keeps the old save.

Both saves hold the seed of the level, how many levels came before it, the game time at its start, the party as it was
then (each character's portrait, as its key and index in the asset mapping, see [DEBUGGING.md](DEBUGGING.md#assets),
gender and hit points, the party's mana and the spells in effect with the time left of each, and the keys of the items
it carried with the number of each stack, see [DEBUGGING.md](DEBUGGING.md#items)), and the layout if it was given with
`--layout`. `save.json` also holds the progress in the level: the game time, the party, its mana, spells and items when
it was saved, where the player was, where they looked and how they moved, how far each door was open and where it
moved, and what each chest held. Noclip isn't saved. Levels are generated again from their seed, so a save of an older
version of the game may give another level, or be refused if the save format changed; a `save.json` whose level has
other numbers of doors or chests than the level generated again can't be loaded.

A run with `--exec` or `--screenshot` (see the Console section of [DEBUGGING.md](DEBUGGING.md#console)) never saves,
not even the levels reached after its commands or when it quits, so that test runs don't replace the player's saves.
They can still load them. They also don't ask whether to continue a save, but start a new game.
