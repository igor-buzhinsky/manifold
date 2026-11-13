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
| Escape | Quit |

## Cheats

| Key | Action |
|---|---|
| Ctrl+Z | Toggle noclip: fly through obstacles and walls, without falling. Fly with Page Up / Page Down |

## Console

Console commands inspect the game, e.g. to test a level. Type them into the terminal the game was started
from, one per line, or give them on the command line with `--exec`, separated by `;`:

```sh
# From high above the middle of the level, looking down: the arrow shows the player through the roof.
./build/manifold 42 --exec "arrow on; camera 0 0 30 90 -90; screenshot top.png 800 450; quit"
```

In `--exec`, a command that fails, or a screenshot that can't be saved, stops the game with an error. When
typed into the terminal, it only shows the error. Commands are read from the terminal only while the game
runs in the foreground of the shell. Words are separated by spaces and can't contain any, so neither can file names.
Angles are in degrees.

| Command | Action |
|---|---|
| `help` | List all commands |
| `level_info` | Describe the level, depending on its layout, and where the player is. For rooms: how tiles map to world coordinates, the start, and a plan of all layers. For the debug layout: all obstacles and ramps |
| `camera` | Show where the camera is |
| `camera X Y Z YAW PITCH` | Put the camera at (X, Y, Z), looking towards YAW (counter-clockwise from +x) and PITCH (from -90, straight down, to 90). The player still moves with the keys |
| `camera player` | Put the camera back at the player's eyes |
| `arrow on\|off` | Show an arrow at the player's feet, pointing where the player faces. It is drawn on top of everything, so it is seen through floors and walls |
| `lighting on\|off` | Turn lights off to see the plain texture colors, or on again |
| `shadows on\|off` | Turn shadows off or on |
| `screenshot FILE [WIDTH HEIGHT]` | Save the view as a PNG file after this frame, by default as large as the window. Other sizes keep the 16:9 view, with black bars |
| `wait FRAMES` | Run the next commands this many frames later: with 1 in the next frame. E.g. to let the player fall |
| `quit` | Quit the game after this frame, without running the commands after it |

Numbers may start with `+` or `-`. `--screenshot FILE.png` is short for `--exec "screenshot FILE.png"`, with
`quit` after all other commands.
