# Manifold — Debugging

How to look into the game and change it without building it again: remapping its assets, the console with its
commands for inspecting and testing levels, and the debug overlay. Playing the game is described in
[GAME_MANUAL.md](GAME_MANUAL.md).

## Assets

`assets/asset_mapping.json` says which sound, texture and portrait files the game uses for what, so they can be
changed without building the game again. Each of its keys has a path, relative to `assets/`, or a list of paths, and
one file can be listed under several keys:

| Key | Files |
|-----|-------|
| `DOOR_OPEN_SOUNDS`, `DOOR_CLOSE_SOUNDS` | OGG sounds of a door that starts to open or to close |
| `CHEST_OPEN_SOUNDS` | OGG sounds of a chest that is activated |
| `EXIT_SOUNDS` | OGG sounds of entering the exit of a level |
| `FOOTSTEP_SOUNDS_GRASS`, `_WOOD`, `_METAL`, `_STONE` | OGG footsteps in levels of each texture kind |
| `LEVEL_TEXTURES_GRASS`, `_WOOD`, `_METAL`, `_STONE` | PNG textures of levels, at most 128 × 128 pixels, by what they show; a level gets one of all of them |
| `PORTRAITS_MALE`, `PORTRAITS_FEMALE`, `PORTRAITS_ANY` | PNG portraits of men, women and anyone (e.g. a skeleton), whose gender is then random |

Doors, chests, exits, levels and characters keep the position of their file in the list of its key, which the level
or the new game chooses at random. A file that is missing or can't be used, e.g. a texture that is too large, gives
no sound, a plain grey level or an empty portrait, and a warning in the console; the files after it keep their
positions. The number of files of each key decides the random choices, so adding or removing files changes the sounds
and textures of seeds and the portraits of new parties. Saved parties keep the positions of their portraits, so
inserting, removing or reordering portraits before a saved one shows another portrait, while adding one at the end
changes nothing.

## Console

Console commands inspect the game, e.g. to test a level. There are three ways to run them:

- In the console window, which the `/~ key opens over the top half of the picture. It shows the log and
  an input line. While it is open, the keys type into it and the player stands still.
- In the terminal the game was started from: type them one per line. They can also be piped into a normal run, which,
  unlike `--exec`, uses the settings (e.g. full screen) and saves, e.g. with a scratch directory for the save:

  ```sh
  (sleep 2; echo "menu close"; echo "screenshot full.png"; sleep 1; echo quit) | XDG_DATA_HOME=/tmp/m ./build/manifold 42
  ```

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
| `level_info` | Describe the level, depending on its layout, its texture (its key in the asset mapping and its index there, see [Assets](#assets)) and its ambient light. For rooms: how tiles map to world coordinates, the start, the exit, how many openings have doors and what they are made of, the chests with their tiles, and a plan of all layers (`@` the start, `E` the exit). For the debug layout: all obstacles, ramps, the exit and the chests |
| `player_info` | Describe the player: where their feet are, yaw and pitch, whether they stand on the ground, the horizontal velocity and its speed, the vertical speed, and whether noclip is on and Levitate in effect |
| `camera` | Show where the camera is |
| `camera X Y Z YAW PITCH` | Put the camera at (X, Y, Z), looking towards YAW (counter-clockwise from +x) and PITCH (from -90, straight down, to 90). The player still moves with the keys |
| `camera player` | Put the camera back at the player's eyes |
| `teleport X Y Z [YAW]` | Move the player's feet to (X, Y, Z), facing YAW if given. Obstacles are not checked, and the player falls from there unless noclip is on |
| `doors` | List the doors of the level: what they are made of, their numbers, where they stand, the direction of their wall, and whether they are open, closed, opening or closing |
| `door N open\|close` | Start to open or close door N, as E does. Fails if the door moves, is already open or closed, or the player stands in it |
| `activate [X Y]` | Activate what the center of the player's view points at, like E, or what a click at (X, Y) would: fractions of the view's width and height from its top-left corner, from 0 to 1. Uses the player's eyes also while `camera` shows another view |
| `walk FRAMES` | Walk the player forward for this many frames, as holding W does, e.g. to hear the footsteps. The commands after it don't wait for it: use `wait`. A walk goes on in the next level, and so does a run |
| `run FRAMES` | Run the player forward for this many frames, as holding Shift and W does, like `walk`. A `walk` or `run` replaces the one before |
| `next_level` | Start the next level, as if the player reached the exit, with the exit's sound. The commands after it run in the next frame, when the new level is set up. A camera put in place with `camera` stays where it is, so `camera player` brings the view back to the player |
| `load` | Load the saved game, as F9 does. The commands after it run in the next frame, in the loaded level. Fails if there is no save or it can't be read. As with `next_level`, a camera put in place with `camera` stays where it is, and a `walk` or `run` goes on in the loaded level |
| `party` | Show the party's mana and the spells in effect with the seconds left of each, and list the characters of the party, from the left: their gender, hit points and portrait (its key in the asset mapping and its index there) |
| `hp N VALUE` | Set the hit points of character N (from 0, see `party`) to VALUE, from 0 to their maximum. A cheat, e.g. to see the red bar below the portrait shrink |
| `cast candlelight\|levitate` | Cast a spell, as its key does (see Spells in [GAME_MANUAL.md](GAME_MANUAL.md#spells)). Fails if the party has less mana than it costs |
| `mana VALUE` | Set the party's mana to VALUE, from 0 to its maximum. A cheat, e.g. to cast spells again |
| `noclip [on\|off]` | Let the player fly through obstacles and walls, without falling, with Page Up / Page Down, or not |
| `cutaway HEIGHT` | Leave out everything above HEIGHT, to look into a layer from a camera above it. Solids that are cut open are dark. For room levels, `level_info` tells the height for each layer |
| `cutaway off` | Show everything again |
| `arrow [on\|off]` | Show an arrow at the player's feet, pointing where the player faces. It is drawn on top of everything, so it is seen through floors and walls, and also when the player is above a `cutaway` |
| `crosshair [on\|off]` | Show the crosshair at the center of the player's view, or not. It is hidden while the camera is put in place with `camera` |
| `lighting [on\|off]` | Turn lights off to see the plain texture colors, or on again |
| `shadows [on\|off]` | Turn shadows off or on |
| `imgui [on\|off]` | Show or hide the debug overlay, like F3 |
| `menu start\|game\|close` | Show the start menu or the game menu (see Menus in [GAME_MANUAL.md](GAME_MANUAL.md#menus)) instead of the open menus, or close them. The commands after it run in the next frame, when the menu is open, so that a screenshot shows it. The game is paused while a menu is open, but commands still run |
| `screenshot FILE [WIDTH HEIGHT]` | Save the view as a PNG file after this frame, by default as large as the window, in full screen the whole screen in its pixels, with black bars if it isn't 16:9. The commands after it run in the next frame, so they don't change the picture. The game goes on for that frame, as with `wait 1`. Other sizes keep the 16:9 view, with black bars |
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
