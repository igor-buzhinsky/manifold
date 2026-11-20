# Manifold — Roadmap

*Manifold* is a working title. It is a hobby first-person fantasy game played by a party of mages.
It looks and feels like *Daggerfall*, but is structured as a linear series of
rogue-like, procedurally generated 3D levels. There is no story or lore yet.

## Goals and constraints

- Runs on Linux, with full-screen support.
- Low performance requirements, no fancy graphics: a low-fi, retro look.
- Textures and models come from free sources or are AI-generated.
- Believable lighting is a core feature: dark levels lit by the player's light spell
  and by multiple other light sources.
- Open and non-commercial.

## Technical decisions

### Language and build

- **C++20**.
- **CMake** with Ninja, pulling dependencies with FetchContent. Only the development packages that SDL3 needs come
  from the system.
- **clang-format** and **clang-tidy** enforce the style (see [CODING_STYLE.md](CODING_STYLE.md)).
- **GoogleTest** for unit tests. Game logic lives in the `manifold_core` library, which has no window or
  OpenGL code, so it can be tested.

### Libraries

| Concern | Choice | Status | Notes |
|---|---|---|---|
| Window, input, full-screen, audio | **SDL3** | In use | Built from source as a static library. Works on Wayland and X11. |
| Rendering API | **OpenGL 4.1 core** | In use | The newest version on macOS. 4.5 is considered, see [DECISIONS.md](DECISIONS.md). Vulkan is not worth the effort for this scope. |
| GL loader | **glad 2** | In use | Generated for OpenGL 4.1 core and kept in `external/glad/`, see [DECISIONS.md](DECISIONS.md). |
| Math | **GLM** | In use | |
| Images | **stb_image**, **stb_image_write** | In use | stb_image loads textures (PNG only), stb_image_write saves screenshots. |
| Models | **cgltf** (glTF 2.0) | Planned | glTF is the single asset format. Blender converts other formats to it. |
| Collision | Own code | In use | See below. **Jolt Physics** is considered, see [DECISIONS.md](DECISIONS.md). |
| Debug UI | **Dear ImGui** | In use | The debug overlay (F3): live tuning of lights, frame times, player state. |
| Game UI | Own code | In use | Menus, later the party screen: own controls drawn with the console's blocky font, see [DECISIONS.md](DECISIONS.md). |
| Navigation | **Recast/Detour** | Later | Navmeshes for monsters, including multi-layer levels. |
| Sound decoding | **stb_vorbis** | In use | OGG Vorbis, decoded when the game or a level starts. SDL3 mixes up to 4 sounds at a time. |
| Save files | **nlohmann/json** | In use | Saves are plain JSON, see [DECISIONS.md](DECISIONS.md). |

Rejected alternatives:

- **raylib**: the fastest start, but HDR and multi-light shadows would mean working around it.
- **Godot**: gives most features for free, but most code would be GDScript instead of C++.

### Architecture principles

The core is fully 3D, so that water, levitation and arbitrary 3D objects can be added later without
rewrites.

- All positions, velocities and transforms are `vec3`. Nothing in the game logic assumes `(x, y)`.
- No grid or raycaster renderer. The renderer draws arbitrary triangle meshes.
- **Collision is our own code**, not Jolt. The player is a vertical cylinder that moves and slides against
  rotated boxes, ramps and the level bounds, in 3D. Gravity, jumping, landing, and sticking to ramps
  when walking down them work on top of this. This was simpler than Jolt for the shapes needed so far,
  and is good C++ practice. The world sorts obstacles, ramps and doors into grids of 2 m cells, so each move
  and ray cast tests only those near the player or the ray. Replacing it with Jolt Physics is considered,
  see [DECISIONS.md](DECISIONS.md).
- A level is a 3D data structure: obstacles, ramps, bounds, gravity and the player start (position and yaw). Later also
  lights and volumes (for example water). Generators may work on a 2D grid internally, but nothing
  downstream depends on that.
- Level generation is deterministic from a seed, so levels can be reproduced for debugging. All random choices are
  made with the helpers of `src/util/random.hpp`, whose results are the same on every platform (see
  [DECISIONS.md](DECISIONS.md)).
- Each level can have its own gravity. The jump take-off speed is the same everywhere, so the player
  jumps higher where gravity is weaker.

### Rendering and lighting principles

- **Linear-space lighting** from the start, converted to sRGB for the display.
- **Tonemapping** (Reinhard, for now in the main shader). A level with zero ambient light is truly dark.
  A separate HDR render target can come when post-processing needs it.
- Point lights with inverse-square falloff and a smooth range cutoff.
- Omnidirectional shadows from cubemap shadow maps, limited to the nearest N lights. (The first plan was
  about 4–8. Static lights made more affordable: all 31 torch lights in use cast shadows, see v0.2.)
- Forward rendering first. Move to clustered forward or deferred only if the light count requires it.
- Retro look: low-res textures with nearest-neighbour filtering, optional low-res render target,
  distance fog, billboard sprites for items and creatures.

## v0.1 — Exploration prototype (finished: 2026-11-11, commit 3d75613)

Scope: walk around a randomly generated level in the dark with a light at the player. Done:

- SDL3 window with an OpenGL 3.3 context. The view keeps a fixed aspect ratio inside the window.
  `--screenshot FILE.png` saves the first frame and exits, which helps to check rendering.
- First-person camera: keyboard turning and looking up and down (at most 30°), WASD movement.
- **Gravity and jumping**: one jump per Page Up press; the height depends on the level's gravity.
- **Collision** with sliding along walls, ramps that can be walked up and down, and a noclip cheat
  for flying through everything.
- **Level layouts**, chosen at random with configurable probabilities, or with `--layout`:
  - **Rooms** (the default): rectangular rooms on a grid of 2 m tiles, connected by openings in their
    walls, in three layers on top of each other. Holes of 1 × 2 tiles lead down to the next layer, and
    some of them have a ramp under them that leads back up. A text plan of the level is printed.
  - **Debug**: scattered rotated blocks on the floor and under the ceiling, plus ramps, in an open area.
  - The player never starts inside an obstacle, and all rooms are reachable.
- **Lighting**: one warm point light at the player's eyes plus a small ambient term. Linear-space
  lighting, Reinhard tonemapping and sRGB output. A 1 m checker pattern on all surfaces stands in for
  textures and shows scale (replaced by textures in v0.2).
  - Shadows are not needed yet. A light at the camera position lights exactly the surfaces the camera
    can see, so its shadows are never visible. This changes once the light is offset (for example held
    in a hand) or other lights are added.
- Logging to stderr with info, warning and error levels.
- Unit tests for geometry, collision, level generation, game logic and logging.

## v0.2 — Lighting and level structure (finished: 2026-11-15, commit bf38f65)

Scope: lights, shadows and textures in levels of several layers, a series of levels connected by exits, doors,
and tools for inspecting the game. Done:

- **OpenGL 4.1**: the renderer moved from 3.3 core to 4.1 core, which is as far as macOS goes, and loads OpenGL
  with glad instead of libepoxy, which helps Windows and macOS support (see [DECISIONS.md](DECISIONS.md)).
- **Console**: commands that inspect the game, for testing levels by hand and for scripted checks, see
  [GAME_MANUAL.md](../GAME_MANUAL.md). They are typed into the in-game console window (the `/~ key), into
  the terminal the game was started from, or given with `--exec`; `--screenshot` is short for a script.
  Commands: `help`, `level_info` (its text depends on the layout), `camera`, `teleport`, `next_level`,
  `noclip` (no longer a key of its own), `cutaway` (leaves out everything above a height, to look into a layer
  from above), `arrow`, `crosshair`, `lighting`, `shadows`, `imgui`, `doors`, `door`, `activate`, `screenshot`
  (of any size, drawn off screen), `wait` and `quit`. The console window shows the log and has an input line with
  history. Its parts are reusable for the game's interface later: `TextField` (a line of typed text),
  `OverlayMesh` (rectangles and text in screen pixels, with the blocky stb_easy_font in cells of equal width) and
  `OverlayRenderer`.
- **Level exit**: a cube floating above the floor, drawn as a portal in its own draw call: it spins slowly, glows
  without lighting, and its faces show a pixelated blue swirl with sparkles inside a bright frame. The darker parts
  of the swirl, and faces seen from the side, are partly see-through. A slowly pulsing blue light, which casts
  shadows, sits at its center, and the portal pulses with it. Touching the cube starts the next level, with a seed
  derived from the current one and the layout chosen as for the first level (`--layout` applies only to the first
  level), which is a minimal version of the linear series of levels. Room levels put it in the center of a random
  floor tile on another layer than the start, away from holes and openings (`E` on the plan); the debug layout puts
  it anywhere free. `next_level` starts the next level from the console.
- **Doors between rooms** (see [DECISIONS.md](DECISIONS.md)): each opening of a room level gets a door with a
  chance chosen for the level from 10% to 100%. A closed door blocks movement and light. A door is two halves with
  a texture made in code, between two dark frames that are static obstacles. All doors of a level are of wood or
  all of metal, each with a chance of 50%. E (or the `activate` command) casts a ray from the eyes up to 2 m: on a
  closed door it opens it, on a frame it opens or closes the door, and on a moving door it does nothing. The halves
  slide into the walls in one second, and the door blocks the player until it is fully open. It doesn't close while
  the player stands in it. The halves cast shadows: while a door moves, the shadow maps of the lights that reach it
  are drawn again in every frame. A thin white crosshair with a black outline marks the center of the view that E
  activates.
- **Multiple point lights**: torches, static light sources with color and flicker. Room levels get torches on
  random wall sides facing a room (5–10% of them, random for each level), debug levels on their outer walls. All
  torches of a level have one flame color: orange (50%), blue or green (25% each). A torch is a six-sided handle in
  the world mesh (untextured, with a per-vertex color) and a flame sprite: an upright quad facing the camera,
  painted by a shader as a pixelated, flickering teardrop. Each frame, the player's light and the 31 torch lights
  nearest to the player (by distance minus range) go to the shader in a uniform buffer; the light of the level exit
  competes with the torches for these places. Torch lights flicker and fade out smoothly at 6 m (9 m since v0.3, see
  Torch range below). The player's light
  also has a range (15 m), and every light treats surfaces closer than 0.3 m as 0.3 m away, so the wall behind a
  flame doesn't turn white. Lights don't pop when they enter or leave the nearest 31: when lights are left out, the
  gap (distance minus range) of the first one left out is the cut, and chosen lights fade out over the last 2 m
  before it. A light that is replaced has faded out fully, and the one that takes its place comes in from zero.
  This needs no state between frames.
- **Shadows**: cubemap shadow maps for torch lights and the light of the level exit, up to 31 at once, all in one
  cubemap array texture, with each light's six faces drawn in separate passes. The player's light casts none,
  because it is at the camera, where its shadows can't be seen (and the player has no body yet). Torches and the
  world don't move, so each torch's shadow map is drawn once, when the torch becomes one of the nearest, and kept
  in one of 48 slots of a 256 × 256 depth cube map array (about 38 MB). The slot used longest ago is reused.
  Drawing all 31 maps on the first frame takes about 0.2 s. A map stores the distance to the faces that point away
  from the light (back faces), divided by the range, so lit surfaces never shadow themselves and a 5 mm bias is
  enough. For this, the shadow pass also draws faces that the visible mesh leaves out: a shell behind the
  one-sided floor, ceiling and boundary walls, and the hidden tops and bottoms of blocks and ramps. Lookups use
  hardware comparison with 2 × 2 linear filtering. Torch handles and the player cast no shadows (see "Later
  (unordered, low priority, might not be done)").
- **Textures**: each level uses one random texture (chosen from the level seed) for walls, floor and ceiling,
  instead of the checker pattern, from the PNG files under `assets/textures/`, at most 128 × 128 pixels, with
  nearest-neighbour filtering.
- **Dear ImGui debug overlay**: F3 or `imgui [on|off]` shows a window with the frame times, what the renderer
  drew, the player's state, the view switches, and sliders for the lights and the shadow bias. The lights are a
  `LightSettings` in `ViewSettings`, so screenshots use them too. A shadow map is drawn again when the range of its
  light changes.

## v0.3 — Game menu, party and rudimentary spellcasting (finished: 2026-11-20, commit 650f5e2)

Scope: menus, saving and loading, sound, a party of characters with mana and the first spells. Done:

- **Ambient light**: half of the levels (chosen from the level seed) have no ambient light, the others have
  ambient light of the hue of the level's texture (its mean linear color scaled to a set luminance, so that
  light and dark textures give equally bright light), which lights the level with no visible source.
  `level_info` shows it.
- **Faster collision**: the world sorts its obstacles, ramps and doors into grids of 2 m cells (`SpatialGrid`), and
  collision tests only those in the cells under the player, instead of all of them (about 12k obstacles and 660
  doors in room levels). A move takes about 0.5 µs instead of 58 µs. Ray casts test only the obstacles and doors in
  the cells under the box around the ray, which is small for short rays such as the reach for activating doors.
- **Audio: footsteps, entering portals, doors**. Portals: each level picks its exit sound among the OGG files in
  `assets/sounds/portal/` from the level seed, and plays it when the player enters the exit (SDL3 audio, decoded with stb_vorbis). Doors: level generation gives each door a random open and close sound, from `assets/sounds/door_open/`
  and `assets/sounds/door_close/`, played when the door starts to move. Footsteps: the directory of the level's texture (grass, wood, metal or stone) gives the level's texture kind, and walking plays a random footstep of that kind every
  metre.
- **Saving and loading**: the game saves itself when it starts and when the player reaches a new level, in one save
  (`save.json` in the user's data directory, `~/.local/share/manifold/` on Linux), so a level that is left can't be
  returned to. F9 or the `load` command starts the saved level again from its start. A save holds the level seed, the
  level number, the game time at the level's start and a layout given with `--layout`; the level is generated again from them. Runs with `--exec` don't save, so test runs keep the player's save.
- **Game menu and start menu**: Escape opens a vertical menu over the dimmed game, which is paused while it is open:
  Resume, New game, Load game and Exit game. Ctrl+Q quits at once. When the game starts and there is a save, a menu of
  the same style on black asks whether to continue it, to start a new game, which replaces the save only once it is
  chosen, or to exit. Menus are used with the arrow keys and Enter or with the mouse. They are built of the game's own controls
  (see [DECISIONS.md](DECISIONS.md)): a stack of screens (`ScreenStack`), each a `Screen` that lays itself out anew
  for every event and drawing; `MenuScreen` with a `ButtonList`, drawn as an `OverlayMesh` in shared colors
  (`ui_style.hpp`). Later screens, such as the party screen with tabs, add controls to this. The `menu` command opens
  and closes menus, and screenshots show them.
- **Activating with the mouse**: a click with the left button over the game activates what the clicked point of the
  view shows, as E does at the center: a ray from the eyes through that point (`Camera::ray_direction`), with the
  same 2 m reach. Since the point can be anywhere in the view, clicks reach things that E can't, beyond the 30° that
  the view can be turned up or down, which is fine for now. Clicks are ignored while a camera put in place with
  `camera` shows the view. `activate X Y` does the same from the console.
- **Basic party implementation** (see [DECISIONS.md](DECISIONS.md)): a new game makes a party of 3 characters, which
  moves as one body, the player. Each gets another portrait, chosen at random from the game's seed among the PNG files
  in `assets/portraits/male/`, `female/` and `any/`, and the gender of its directory, or a random one in `any/`. If
  there are too few portraits, a warning is logged and the last characters get none. Each character has current and
  maximum hit points (100), which nothing lowers yet. The portraits are drawn scaled down in the bottom-left corner of
  the screen, side by side with small gaps (`layout_party_hud()`, textured by `ImageRenderer`), with a thin red bar of
  hit points below each, or an empty dark place for a missing portrait. The save holds the party at the start of the
  level: portraits, genders and hit points. `party` lists the characters, and the `hp` cheat sets
  their hit points.
- **Portable random choices** (see [DECISIONS.md](DECISIONS.md)): all random choices made from a seed (levels,
  textures, sounds, ambient light, the party) use the helpers of `src/util/random.hpp` on the raw output of
  `std::mt19937_64`, instead of the standard distributions and `std::shuffle`, whose results differ between standard
  libraries. So the random choices of a seed are the same on Linux, Windows and macOS. Levels may still differ, since
  the math functions such as `std::cos`, which level generation also uses, may differ in their last bits between
  platforms (see Windows and macOS support below). A test (`PortableRandomChoices`) fails if a source uses the
  standard tools, and `random_test.cpp` pins the results for a seed. Levels of a seed are not those of before, so the
  save format version is 3, which also refuses saves of version 2, the first with a party.
- **Spells and mana** (see [DECISIONS.md](DECISIONS.md)): the party has shared mana (`PartyMagic`), 100 of 100 at the
  start, which regains 1 per second of game time. Spells are effects that last 60 seconds: casting one costs mana and
  starts its duration, also again while it is in effect, without adding what was left. Without enough mana, a cast does
  nothing. **Candlelight** (C, cost 5) is the party's light at the eyes, which was always on before, so a new game
  starts darker; without it the torches get its place among the lights drawn. **Levitate** (L, cost 10) turns off
  gravity: Page Up / Page Down fly as with noclip, but with collision, without footsteps and sticking to ramps; when it
  ends in the air, the party falls from rest. Loading moved from L to F9. The spells in effect are listed in white
  boxes in the top-right corner, and a blue bar in the bottom-right corner shows the mana (`spell_hud.hpp`). The save
  holds the mana and the spells in effect at the start of the level, with the time left of each, so the save format
  version is 4. The `cast` command casts a spell as its key does, the `mana` cheat sets the mana, and `party` shows
  both.
- **A torch at the start**: the first level of a game gets an extra torch on the wall nearest to where the player
  starts, also when the start room has torches or the level has ambient light, so the party isn't in the dark before
  casting Candlelight. The room layout keeps the wall sides of the start room that got no torch
  (`Level::free_start_torches`), and `generate_level()` adds the one nearest to the start, choosing among equally near
  ones with its own random numbers. So the rest of the level, and all other levels, stay as they were. The debug
  layout has no rooms and gets no such torch. Loading a save of a first level gives the torch again.
- **Torch range** of 9 m instead of 6 m, so rooms are less dark. The light changes little within 3 m, so the gain is in
  the far parts of rooms. More torches now reach a door, whose shadow maps are drawn again in every frame while it
  moves (see Fewer shadow maps per frame below), and more torches compete for the places of the lights drawn.
- **Spells that end soon**: the box of a spell blinks in its last 10 seconds, shown for half a second and hidden for
  half a second, by the spell's time left in game time, so it stops while the game is paused (`spell_box_visible()`).
  A hidden box keeps its place, so the boxes below it don't move.

## v0.4 — Water, rudimentary monsters and inventory

- TODO
  - Monsters as billboard sprites, with Recast/Detour navigation.
  - **Water**: water volumes in levels, transparent water surfaces, underwater fog and tint.
  - **Swimming and diving** (including breath), buoyancy for floating objects.
  - **Static 3D props** loaded from glTF, with simple collision shapes (boxes, cylinders, hulls): furniture and
    decoration such as tables, crates, pillars and statues, and models for braziers, crystals and items, made in
    Blender or taken from free packs. Props don't move, so they can join the level mesh and cast shadows like walls.
    This needs several textures per level, placement rules that keep doorways, ramps, the start and the exit free,
    and new collision shapes (or Jolt). A first version can use a few box-shaped props with box collision.
  - Items and inventory. Items lying on the level are implemented either as 3D objects (preferred) or billboard
    sprites.
  - **Damage from falling**: a fall from high enough hurts the characters of the party.
  - **A single casting sound for spells**: one sound that plays whenever a spell is cast.

## Later (unordered)

- **OpenGL 4.5** (considered, see [DECISIONS.md](DECISIONS.md)): debug output through the logger, Direct State
  Access, shader storage buffers and compute shaders. A shader storage buffer could hold all lights of the level,
  and a compute shader could choose the lights for each part of the screen. This closes the macOS path unless we
  keep a 4.1 render path.
- **Jolt Physics** (considered, see [DECISIONS.md](DECISIONS.md)): replace our collision code with Jolt, with the
  player as a `CharacterVirtual`. Keep the current behavior: sliding, ramps, gravity, jumping and noclip.
- **Level exit far from the start**: place the exit where it is far from the start and surely reachable.
- **Fewer shadow maps per frame**: a limit on how many shadow maps are drawn in one frame, so that many torches at
  once don't cause a stutter. While a door moves, only the cube faces that can see it could be drawn again.
- **More 3D level structure**: stairs (?), pits and bridges in generated levels.
- Full-screen mode, and mouse look.
- Spells chosen from a spell book or a quick bar instead of a key each, and a sign when a cast fails.
- Attack spells, spell effects, projectiles that emit light (?).
- Audio: ambient sound, spell sounds.
- Visibility culling (rooms and portals) if levels get large.
- **macOS support**: possible as long as the renderer runs on OpenGL 4.1 (Apple's newest, and
  deprecated, version). Needs testing on a Mac. If we move to OpenGL 4.5, macOS needs a separate 4.1
  path or another renderer.
- **Windows support**: add MSVC warning flags, and send logs to a file or console. SDL3, GLM, stb, glad
  and the C++ code are already portable, except reading console commands from the terminal, which uses POSIX
  `poll()` (`src/platform/terminal_input.cpp`).
- **The same levels on every platform**: random choices already are the same everywhere (`src/util/random.hpp`), but
  level generation also uses math functions such as `std::sin` and `std::cos` (e.g. `bounding_half_size()` in
  `src/world/geometry.cpp`, which decides whether a block of a debug level fits), whose last bits can differ between C
  libraries. Before saves move between platforms, compare the levels of many seeds on each platform, and make
  generation independent of such differences where they show, e.g. by deciding on the tile grid.
- **Stamina and sanity** of each character of the party, next to its hit points.
- **Advanced party implementation**: The party is bound to a magical orb (?), and characters can transport
  themselves to a pocket dimension.
- New level layouts: areas with floating platforms and no walls, open areas, pseudo-platformer levels.
- Story and lore.
- **Error-proof level generation**: generating a level can fail, e.g. when the holes, the player start or the
  exit of a room level don't fit after `max_plan_attempts` attempts, or when a debug level has no room for a
  block or the exit. Generation then throws, and the game quits, also when the player reaches the exit of a
  level that was fine. Trying again with the same seed fails again. Generation should always give a level,
  e.g. by trying other seeds or relaxing the settings, or by construction.
- Better abstraction of level planning, ideally with separation of the room graph and 3D placement.
- Secrets.
- Two or more exit portals per level (different colors) with non-equivalent next level generation, and a
  possibility to know the distinction by finding secrets.
- Pixelation (of textures or the entire visible region as a whole).

## Later (unordered, low priority, might not be done)

- **More static light sources**: braziers and glowing crystals, next to the torches.
- **Shadows of torches**: torch handles cast no shadows now. Under a point light just above the stick,
  a handle throws a hard, long shadow down the wall that a real torch, with its large flame, doesn't.
  A good-looking version needs a softer or larger light source.
- **Level and texture inspectors in the debug overlay**: a tree of the level (layers, rooms, obstacles,
  ramps, torches), where picking an item highlights it or moves the camera there, and views of GPU textures
  (the level texture, the faces of a torch's shadow map, the shadow map slots and which light each holds).
  This would make generated levels and shadow bugs faster to understand than with `level_info` and
  screenshots. Later, a panel with the generator's parameters and a button that generates the level again
  from a new seed, without restarting.
- **Shadows of the player**: the player's light is at the camera, so its own shadows can't be seen, and
  the player has no body to cast shadows from other lights. Once there is a body (or a party), it could
  cast shadows from torches, which needs shadow maps that are drawn again whenever it moves.
- **Different textures** for walls, floor and ceiling.
