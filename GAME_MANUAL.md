# Manifold — Game Manual

## Controls

| Key | Action |
|---|---|
| W / S | Walk forward / backward |
| A / D | Step left / right |
| Left / Right arrow | Turn left / right |
| Up / Down arrow | Look up / down (at most 30° each way) |
| Page Up | Jump (0.5 m high under Earth gravity, higher where gravity is weaker). Press again for the next jump. With noclip or Levitate: fly up |
| Page Down | With noclip or Levitate: fly down |
| E | Activate what the center of the view points at, up to 2 m away: open a closed door, or open or close a door by aiming at one of its dark frames. Press again for the next activation |
| Left mouse button | Activate what the clicked point of the view shows, as E does when aiming at it: from the eyes, up to 2 m away. The point can be anywhere in the view, also beyond the 30° that the view can be turned up or down. Each click activates once. Clicks on the portraits of the party, or between them, on the spells in effect or between them, and on the bar of mana do nothing, nor do clicks while the console is open, while the debug overlay has the keyboard, and while `camera` shows the view from elsewhere |
| C | Cast Candlelight, see Spells below |
| L | Cast Levitate, see Spells below |
| F9 | Load the saved game: start the saved level again from its start, see Saving below |
| `/~ (left of 1) | Open or close the console |
| F3 | Show or hide the debug overlay, see below |
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
shows what a click activates. The `crosshair` command hides or shows the crosshair.

The spells in effect are listed in the top-right corner, each name in a white box, in the order they were first cast.
The blue bar in the bottom-right corner shows the party's mana. Clicks on them, or between the boxes, don't activate
what is behind.

## Party

The player leads a party of three characters, who move together. Their portraits are in the bottom-left corner of the
screen, with a thin red bar below each that shows how many of the character's hit points are left. Each new game
chooses the portraits at random, all different, and each character's gender goes with the portrait. Nothing lowers the
hit points yet, except the `hp` cheat in the console. Clicking a portrait, or between them, does nothing yet, and doesn't activate what is behind it. If the game has fewer
portraits than characters, the last characters have an empty dark place instead.

## Spells

The party shares its mana: 100 at the start of a game, which is also the most it can have. It regains 1 mana per second,
also while spells are in effect, but not while the game is paused. Each spell costs mana when it is cast and lasts 60
seconds of game time. Casting a spell that is in effect starts its 60 seconds again; the time that was left is not
added. If the party has less mana than a spell costs, nothing happens.

| Spell | Key | Cost | Effect |
|---|---|---|---|
| Candlelight | C | 5 | A warm light at the party's eyes. Without it the party has no light of its own, and sees only by the torches, the portal and the level's ambient light, if any |
| Levitate | L | 10 | No gravity: Page Up and Page Down fly up and down, and the party floats where it is. Walls, floors, ceilings and closed doors still stop it, unlike noclip. It makes no footsteps. When the spell ends in the air, the party falls |

Mana and the spells in effect stay as they are in the next level. The `cast` and `mana` commands of the console cast
spells and set the mana, see Console below.

## Levels

Each level has an exit: a spinning blue portal cube floating above the floor, whose pulsing light shows
the way to it. In room levels it is on another layer than the start. Touching it, also with noclip, starts
the next level, with the sound of the portal. Each level's portal has its own sound, picked from the level seed.

Walking on the ground makes a footstep about every metre, and so does landing from a fall of more than about 0.2 m,
such as a jump onto the same floor. The steps sound like what
the level's texture shows: grass, wood, metal or stone (also bricks, tiles, plaster and dirt).

In room levels, some openings between rooms (10% to 100% of them, random for each level) have doors: two
halves between two dark frames. All doors of a level are wooden or all are metal, each with a chance of 50%.
A closed door blocks the way. E on the door opens it: the halves slide into the walls within a second, and
only then can the player pass. To close an open door, aim at one of its frames and press E. Clicking the door or
a frame does the same. A door that is moving ignores E and clicks, and a door doesn't close while the player stands
in it.
A door makes a sound when it starts to open and when it starts to close, also with the `door` command. Each door
has its own two sounds, picked when the level is generated.

## Saving

The game saves itself when it starts and when the player reaches a new level. There is only one save, of the
level the player is in, so a level that is left can't be returned to, and there are no saves made by hand. F9 loads
the save: the saved level starts again from its start. When the game starts and there is a save, it asks
whether to continue it or to start a new game, which replaces the save, see Menus above. New game in the game menu
also replaces it.

The save is `save.json` in the game's data directory: `$XDG_DATA_HOME/manifold/` on Linux, by default
`~/.local/share/manifold/`. It holds the seed of the level, how many levels came before it, the game time at its start, the
party as it was then (each character's portrait, gender and hit points, the party's mana and the spells in effect
with the time left of each), and the layout if it was given with
`--layout`. Levels are generated again from their seed, so a save of an older
version of the game may give another level, or be refused if the save format changed.

A run with `--exec` or `--screenshot` (see Console below) never saves, not even the levels reached after its commands, so that
test runs don't replace the player's save. They can still load it. They also don't ask whether to continue the save,
but start a new game.

## Console

Console commands inspect the game, e.g. to test a level. There are three ways to run them:

- In the console window, which the `/~ key opens over the top half of the picture. It shows the log and
  an input line. While it is open, the keys type into it and the player stands still.
- In the terminal the game was started from: type them one per line.
- On the command line with `--exec`, separated by `;`:

```sh
# From high above the middle of the level, looking down into the second layer. The arrow shows where the
# player is, also when the player is in another layer.
./build/manifold 42 --exec "arrow on; cutaway 6.2; camera 0 0 40 90 -90; screenshot top.png 800 450; quit"
```

In `--exec`, a command that fails, or a screenshot that can't be saved, stops the game with an error. When
typed into the console window or the terminal, it only shows the error. Commands are read from the terminal
only while the game runs in the foreground of the shell. Words are separated by spaces and can't contain
any, so neither can file names. Angles are in degrees. Screenshots don't show the console window, but show
the crosshair, the portraits of the party, the spells in effect, the bar of mana and an open menu.

Keys of the console window:

| Key | Action |
|---|---|
| Enter | Run the command |
| Left / Right, Home / End | Move the cursor |
| Backspace / Delete | Delete the character before / after the cursor |
| Up / Down | Go back / forward in the commands run before |
| Page Up / Page Down | Show older / newer output |
| `/~ or Escape | Close the console |

The console font has only ASCII characters. The `/~ key only opens and closes the console, so the character
it types can't be typed.

| Command | Action |
|---|---|
| `help` | List all commands |
| `level_info` | Describe the level, depending on its layout, its ambient light, and where the player is. For rooms: how tiles map to world coordinates, the start, the exit, how many openings have doors and what they are made of, and a plan of all layers (`@` the start, `E` the exit). For the debug layout: all obstacles, ramps and the exit |
| `camera` | Show where the camera is |
| `camera X Y Z YAW PITCH` | Put the camera at (X, Y, Z), looking towards YAW (counter-clockwise from +x) and PITCH (from -90, straight down, to 90). The player still moves with the keys |
| `camera player` | Put the camera back at the player's eyes |
| `teleport X Y Z [YAW]` | Move the player's feet to (X, Y, Z), facing YAW if given. Obstacles are not checked, and the player falls from there unless noclip is on |
| `doors` | List the doors of the level: what they are made of, their numbers, where they stand, the direction of their wall, and whether they are open, closed, opening or closing |
| `door N open\|close` | Start to open or close door N, as E does. Fails if the door moves, is already open or closed, or the player stands in it |
| `activate [X Y]` | Activate what the center of the player's view points at, like E, or what a click at (X, Y) would: fractions of the view's width and height from its top-left corner, from 0 to 1. Uses the player's eyes also while `camera` shows another view |
| `walk FRAMES` | Walk the player forward for this many frames, as holding W does, e.g. to hear the footsteps. The commands after it don't wait for it: use `wait`. A walk goes on in the next level |
| `next_level` | Start the next level, as if the player reached the exit, with the exit's sound. The commands after it run in the next frame, when the new level is set up. A camera put in place with `camera` stays where it is, so `camera player` brings the view back to the player |
| `load` | Load the saved game, as F9 does. The commands after it run in the next frame, in the loaded level. Fails if there is no save or it can't be read. As with `next_level`, a camera put in place with `camera` stays where it is, and a `walk` goes on in the loaded level |
| `party` | Show the party's mana and the spells in effect with the seconds left of each, and list the characters of the party, from the left: their gender, hit points and portrait |
| `hp N VALUE` | Set the hit points of character N (from 0, see `party`) to VALUE, from 0 to their maximum. A cheat, e.g. to see the red bar below the portrait shrink |
| `cast candlelight\|levitate` | Cast a spell, as its key does (see Spells above). Fails if the party has less mana than it costs |
| `mana VALUE` | Set the party's mana to VALUE, from 0 to its maximum. A cheat, e.g. to cast spells again |
| `noclip [on\|off]` | Let the player fly through obstacles and walls, without falling, with Page Up / Page Down, or not |
| `cutaway HEIGHT` | Leave out everything above HEIGHT, to look into a layer from a camera above it. Solids that are cut open are dark. For room levels, `level_info` tells the height for each layer |
| `cutaway off` | Show everything again |
| `arrow [on\|off]` | Show an arrow at the player's feet, pointing where the player faces. It is drawn on top of everything, so it is seen through floors and walls, and also when the player is above a `cutaway` |
| `crosshair [on\|off]` | Show the crosshair at the center of the player's view, or not. It is hidden while the camera is put in place with `camera` |
| `lighting [on\|off]` | Turn lights off to see the plain texture colors, or on again |
| `shadows [on\|off]` | Turn shadows off or on |
| `imgui [on\|off]` | Show or hide the debug overlay, like F3 |
| `menu start\|game\|close` | Show the start menu or the game menu (see Menus above) instead of the open menus, or close them. The commands after it run in the next frame, when the menu is open, so that a screenshot shows it. The game is paused while a menu is open, but commands still run |
| `screenshot FILE [WIDTH HEIGHT]` | Save the view as a PNG file after this frame, by default as large as the window. Other sizes keep the 16:9 view, with black bars |
| `wait FRAMES` | Run the next commands this many frames later: with 1 in the next frame. E.g. to let the player fall |
| `quit` | Quit the game after this frame, without running the commands after it |

Commands that take `[on|off]` toggle the setting when run without it, and log the new state. Numbers may
start with `+` or `-`. `--screenshot FILE.png` is short for `--exec "screenshot FILE.png"`, with
`quit` after all other commands.

## Debug overlay

F3 or the `imgui` command shows a window in the top right corner that is used with the mouse:

- **Frame**: frames per second, a graph of the last frame times, and how many lights were drawn, how many
  of them with shadows, and how many shadow maps were drawn in this frame.
- **Player**: position, yaw and pitch, vertical speed, whether the player stands on something, the party's
  mana, whether Levitate is in effect, and a checkbox for noclip.
- **View**: checkboxes for the arrow, the crosshair, lighting and shadows, as the commands of the same names
  set them.
- **Lights**: the ambient light; the color, intensity and range of the player's light; the intensity and
  range of the torches; the intensity and range of the exit's light; the most lights drawn at once, the
  distance over which they fade out before they are left out, and the shadow bias. Reset lights puts them
  back to the defaults, with the level's ambient light. Each new level sets the ambient light again.

Ctrl+click on a slider to type a value. While a value is typed, or a control is held with the mouse, the
keys don't move the player and don't open the console; only F3 still works. Clicks on the overlay don't
activate what the game shows behind it. The overlay can't be used while
the console is open. Screenshots don't show it, but they show the lights it set.
