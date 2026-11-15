# Manifold — Game Manual

## Controls

| Key | Action |
|---|---|
| W / S | Walk forward / backward |
| A / D | Step left / right |
| Left / Right arrow | Turn left / right |
| Up / Down arrow | Look up / down (at most 30° each way) |
| Page Up | Jump (0.5 m high under Earth gravity, higher where gravity is weaker). Press again for the next jump. With noclip or levitation (not implemented yet): fly up |
| Page Down | With noclip or levitation (not implemented yet): fly down |
| E | Activate what the center of the view points at, up to 2 m away: open a closed door, or open or close a door by aiming at one of its dark frames. Press again for the next activation |
| `/~ (left of 1) | Open or close the console |
| F3 | Show or hide the debug overlay, see below |
| Escape | Close the console if it is open, otherwise quit |

## Levels

Each level has an exit: a spinning blue portal cube floating above the floor, whose pulsing light shows
the way to it. In room levels it is on another layer than the start. Touching it, also with noclip, starts
the next level.

In room levels, some openings between rooms (10% to 100% of them, random for each level) have doors: two
wooden halves between two dark frames. A closed door blocks the way. E on the door opens it: the halves slide
into the walls within a second, and only then can the player pass. To close an open door, aim at one of its
frames and press E. A door that is moving ignores E, and a door doesn't close while the player stands in it.

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
any, so neither can file names. Angles are in degrees. Screenshots don't show the console window.

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
| `level_info` | Describe the level, depending on its layout, and where the player is. For rooms: how tiles map to world coordinates, the start, the exit, how many openings have doors, and a plan of all layers (`@` the start, `E` the exit). For the debug layout: all obstacles, ramps and the exit |
| `camera` | Show where the camera is |
| `camera X Y Z YAW PITCH` | Put the camera at (X, Y, Z), looking towards YAW (counter-clockwise from +x) and PITCH (from -90, straight down, to 90). The player still moves with the keys |
| `camera player` | Put the camera back at the player's eyes |
| `teleport X Y Z [YAW]` | Move the player's feet to (X, Y, Z), facing YAW if given. Obstacles are not checked, and the player falls from there unless noclip is on |
| `doors` | List the doors of the level: their numbers, where they stand, the direction of their wall, and whether they are open, closed, opening or closing |
| `door N open\|close` | Start to open or close door N, as E does. Fails if the door moves, is already open or closed, or the player stands in it |
| `activate` | Activate what the center of the player's view points at, like E |
| `next_level` | Start the next level, as if the player reached the exit. A camera put in place with `camera` stays where it is, so `camera player` brings the view back to the player |
| `noclip [on\|off]` | Let the player fly through obstacles and walls, without falling, with Page Up / Page Down, or not |
| `cutaway HEIGHT` | Leave out everything above HEIGHT, to look into a layer from a camera above it. Solids that are cut open are dark. For room levels, `level_info` tells the height for each layer |
| `cutaway off` | Show everything again |
| `arrow [on\|off]` | Show an arrow at the player's feet, pointing where the player faces. It is drawn on top of everything, so it is seen through floors and walls, and also when the player is above a `cutaway` |
| `lighting [on\|off]` | Turn lights off to see the plain texture colors, or on again |
| `shadows [on\|off]` | Turn shadows off or on |
| `imgui [on\|off]` | Show or hide the debug overlay, like F3 |
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
- **Player**: position, yaw and pitch, vertical speed, whether the player stands on something, and a
  checkbox for noclip.
- **View**: checkboxes for the arrow, lighting and shadows, as the commands of the same names set them.
- **Lights**: the ambient light; the color, intensity and range of the player's light; the intensity and
  range of the torches; the intensity and range of the exit's light; the most lights drawn at once, the
  distance over which they fade out before they are left out, and the shadow bias. Reset lights puts them
  back as they were at the start.

Ctrl+click on a slider to type a value. While a value is typed, or a control is held with the mouse, the
keys don't move the player and don't open the console; only F3 still works. The overlay can't be used while
the console is open. Screenshots don't show it, but they show the lights it set.
