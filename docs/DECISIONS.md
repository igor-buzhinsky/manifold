# Manifold — Decisions

Important technical decisions, newest last. Each entry says what was decided, why, and what it costs.
Plans that follow from a decision are in [ROADMAP.md](ROADMAP.md).

Statuses:

- **ACCEPTED**: decided; the work is planned or done.
- **CONSIDERED**: looks useful, but not decided yet. It may be accepted or rejected later.

## 2026-11-11: Use OpenGL 4.1 core instead of 3.3

Status: **ACCEPTED**. Done in v0.2.

Decision: move the renderer from OpenGL 3.3 core to 4.1 core.

Gains:

- **Cubemap arrays** (core since 4.0): shadow maps for several point lights fit in one texture, which suits
  the planned shadows.
- **Separate shader objects**, **program binaries** and more texture units and uniform space than 3.3
  guarantees.
- 4.1 is the newest version that macOS supports, so this upgrade keeps the macOS path open.

Drawbacks:

- GPUs that only support 3.3 can't run the game, for example Intel Sandy Bridge (2011) on Linux. Every
  GPU from about 2012 on supports 4.1.
- The shaders move to `#version 410`, which is a small change.

## 2026-11-11: Use OpenGL 4.5 core instead of 4.1

Status: **CONSIDERED**.

Decision: move the renderer from OpenGL 4.1 core to 4.5 core.

Gains:

- **Debug output** (`glDebugMessageCallback`, 4.3): GL errors and misuse are reported at once, with a
  message, through our logger.
- **Direct State Access** (4.5): buffers, textures and framebuffers can be changed without binding them
  first. The renderer code is shorter and has fewer bugs from hidden GL state.
- **Shader storage buffers and compute shaders** (4.3): a light list of any length, and clustered forward
  lighting, where a compute shader finds the lights that reach each part of the screen. This matters only
  if we have many lights.

Why 4.5 and not 4.6: 4.6 adds little we would use (SPIR-V shaders, anisotropic filtering in core), while
4.5 also runs on Mesa's software renderer (llvmpipe), which VMs and headless CI use.

Drawbacks:

- **macOS can't be supported** while we need 4.5, because Apple stopped at OpenGL 4.1. Supporting it would
  need a separate 4.1 render path, or a Vulkan (MoltenVK) or Metal renderer.
- Virtual machine GPUs (virgl) and some older GPUs with open drivers can't run the game.
- The setup code in the renderer must be rewritten for DSA.

## 2026-11-11: Use Jolt Physics for collision

Status: **CONSIDERED**.

Decision: replace our own collision code with Jolt Physics. The player becomes a Jolt `CharacterVirtual`.

Gains:

- **Character controller**: CharacterVirtual handles step-up on stairs, the steepest walkable slope,
  ground detection and moving platforms.
- **Speed**: Jolt's broad phase only tests shapes near the player. Our code tested every obstacle on each
  move, which is about 12–20k in room levels, until v0.3 added spatial grids, with which collision and ray
  casts test only nearby obstacles.
- **More shapes**: convex hulls, meshes and cylinders, which glTF props need.
- **Ray and shape casts**: for aiming spells, line of sight and AI.
- **Moving bodies**: doors as kinematic bodies, objects that can be pushed around, and buoyancy for
  water.

Drawbacks:

- Most of `physics/collision.cpp`, much of `Game::update` and many tests must be rewritten. Behavior we
  tuned (jump height that doesn't depend on frame rate, sticking to ramps, landing and bump detection)
  must be rebuilt on top of Jolt, which works differently.
- Jolt is a large dependency, so clean builds take longer.
- Jolt has its own math types, so values are converted to and from GLM where the two meet. Jolt is
  y-up by default, but its up direction can be set, so our z-up convention stays.
- Less of the collision algorithm is our own code, which matters for the goal of practicing C++.
- Alternative: keep our own code and add a spatial grid for speed. This was done in v0.3, so speed matters
  less for this decision now, and it is mostly about stairs, props, doors and water, which would each need more
  of our own physics.

## 2026-11-11: Use glad instead of libepoxy to load OpenGL functions

Status: **ACCEPTED**. Done in v0.2.

Decision: replace the system package libepoxy with a glad 2 loader generated for OpenGL 4.1 core, with its
files kept in the repo in `external/glad/`. After the context is created, the window calls
`gladLoadGL` with `SDL_GL_GetProcAddress` and stops with an error if loading fails.

Gains:

- **No system dependency**: libepoxy-dev drops out of the install command. (pkg-config stays, because
  SDL3 uses it to find Wayland, PipeWire and PulseAudio.) The same code builds on Linux, Windows and
  macOS, which helps both of those "later" items.
- **The version is checked at compile time**: the header only declares functions of OpenGL 4.1 core, so
  using a newer function by mistake (for example one from 4.5) doesn't compile. With libepoxy, such code
  compiles and fails only on a GPU or OS without that function, such as macOS.
- Small and simple: one header and one source file, with no system library to find and link.

Drawbacks:

- Generated code (a few thousand lines) lives in the repo. It must be left out of formatting and lint.
- The loader must be generated again (with the glad web service or its Python tool) when the version or
  the extensions change, for example if we move to OpenGL 4.5 or need an extension.
- The loading call is our code: libepoxy finds functions by itself, on first use, and handles GLX and
  EGL. With glad, a missing call or a call before the context exists gives null function pointers.
- Alternative: keep libepoxy and get it from vcpkg or MSYS2 on Windows and from Homebrew on macOS. This
  works, but each platform needs its own setup, and the version is not checked at compile time.
- Alternative: let CMake generate the loader during the build with glad's CMake support. Nothing
  generated is in the repo, but the build then needs Python with Jinja2.

## 2026-11-11: Store assets in plain git, without Git LFS

Status: **ACCEPTED**.

Decision: textures, images and other assets are committed to the repo as normal git files, in their own
directory (`assets/`). Only game-ready files are committed, for example textures already scaled down to
64–128 px. Large originals (such as 2K/4K downloads and work files) stay outside the repo. A file next to
the assets records where each one came from, its license and author (for attribution), and how it was
processed, so that the committed files can be made again.

Gains:

- **Nothing extra to install or set up**: a normal clone has all the assets. With LFS, everyone who
  clones needs `git-lfs`, and without it they get pointer files instead of images. CI and archives would
  also need extra setup.
- **No LFS limits**: hosts limit LFS storage and bandwidth on free plans, and every clone, CI runs
  included, counts toward the bandwidth.
- **The files are small**: a low-res PNG is a few KB to a few tens of KB, so even hundreds of textures add
  only a few MB to the history.

Drawbacks:

- Git keeps every version of a binary file in full, so assets that change often or large files (high-res
  textures, audio, music, 3D models over roughly 1 MB) would make the history grow quickly. If such files
  are needed, or `.git` grows past a few hundred MB, we should move to LFS. Because the assets are in one
  directory, one `.gitattributes` rule can then cover them.
- Moving files that are already in the history to LFS (`git lfs migrate import`) rewrites the history,
  and everyone must clone again. Tracking only new files with LFS doesn't rewrite anything, but the old
  versions stay in the history.
- The originals aren't kept in the repo, so they must be downloaded again (from the recorded source) to
  process an asset differently.

## 2026-11-12: Torches as a mesh handle and a shader flame, with the nearest lights in a uniform buffer

Status: **ACCEPTED**. Done in v0.2. Shadows followed in a separate change, which also stopped the light
from moving with the flicker.

Decision: a torch is a static wall decoration made of two parts, and its light is one of a few dozen
point lights that the CPU chooses each frame.

- **Handle**: two six-sided prisms (the stick and a wider, charred end), added to the world mesh. They are
  untextured: each vertex has a color and a texture weight (0 for the color alone, 1 for the texture).
- **Flame**: a quad that turns around the vertical to face the camera (so it stays upright when looking up
  or down), drawn in a second, instanced pass. A fragment shader paints a pixelated, swaying teardrop in
  three color bands and discards the pixels outside it. The flame is unlit and needs no texture asset.
- **Lights**: the player's light plus the 31 torches whose range comes closest to the player (distance
  minus range) go to the shader in a std140 uniform buffer of 32 lights. Falloff is inverse-square with a
  smooth fade to zero at the range, `(1 - (d / range)^4)^2`, and a minimum distance of 0.3 m.
- **Flicker**: computed on the CPU from the game time and a per-torch phase, as a sum of sines that
  changes the brightness (75–100%). (At first it also moved the light by up to 2 cm. That was dropped
  with shadows, because a torch's shadow map is drawn once.) Animations use the game time wrapped
  into one hour, so floats stay precise during long play.
- **Placement**: in room levels, each wall side that faces a room's floor gets a torch with the same
  chance, except where a ramp runs along it. The chance is random for each level, from 5% to 10%. (At
  first it was always 10%.) In debug levels, torches hang on the outer walls 8 m apart. Torches have no
  collision.
- **Flame colors**: all torches of a level burn with the same flame, chosen at random: orange (50%), or a
  colder, unsettling blue or green (25% each). The flame sets the light's color and the colors of the
  flame sprite. All flames have the same intensity, so the levels differ in brightness on purpose: by
  perceived brightness, a blue level is lit about 45% as brightly as an orange one, and a green level
  about a third more brightly.
- **No torch or player shadows at this point**: torch lights cast shadows of the level, but torch
  handles and the player cast none, and this won't be implemented now. Under a point light just above
  the stick, a handle would throw a hard, long shadow down the wall, and the player has no body yet.
  Both are under "Later (unordered, low priority, might not be done)" in [ROADMAP.md](ROADMAP.md).

Gains:

- **Simple shapes come from code**: prisms (and spheres later) are a few lines in `world_mesh.cpp`, and
  static ones cost nothing extra to draw, because they are part of the one world mesh.
- **The flame never looks dark**: a lit mesh around its own light would face away from it and get no
  light. A sprite drawn without lighting avoids this, and the shader animates it without a sprite sheet.
- **Discarding instead of blending**: flames are opaque where they are drawn, so they need no sorting by
  depth and work with the depth buffer as it is.
- **OpenGL 4.1 is enough**: a uniform buffer of a fixed size works without shader storage buffers or
  compute shaders.
- **Testable**: torch geometry, flicker, the choice of lights and the placement are plain code in
  `manifold_core`, with unit tests.

Drawbacks:

- **Light went through walls and floors** until shadows came: a torch lit the room behind a wall, and
  the ceiling of the layer above.
- **Lights that are about to be left out are dimmed**: at first, a torch's light switched on or off at
  once when it entered or left the nearest 31. Now, when lights are left out, the chosen ones fade out
  over the last 2 m (of distance minus range) before the first light left out, so no light pops. The
  cost: when many torches are near (also on the layers above and below, since nearest is measured in 3D),
  the chosen torches near the cut are dimmed, even ones whose light reaches the player.
- **Nearest is measured in 3D**, so torches on the layer above or below can take light slots from those
  in the player's room.
- Every pixel loops over all 32 lights. This is cheap at the current resolution, but the count can't grow
  much without clustered lighting (see the OpenGL 4.5 decision).
- The handle is drawn with flat colors, while the walls are textured. Later, a glTF prop could replace
  it.
- Alternatives: an emissive low-poly flame (a solid shape, which has to be animated by changing its
  shape on the CPU or GPU, or looks static), or a flame sprite sheet from a CC0 pack (an asset to find and
  credit, and frames to play through).

## 2026-11-15: Level exit as a spinning, see-through portal cube that leads to a new level

Status: **ACCEPTED**. Done in v0.2. Placing the exit far from the start, where it is surely reachable, is
left for later.

Decision: every level has one exit, a portal cube floating above the floor. Touching it generates the next
level, which is a minimal version of the linear series of levels.

- **Placement**: in room levels, the exit is in the center of a random floor tile on another layer than the
  start, so the player has to find the way there. The tile is not part of a hole in its floor or ceiling, and
  doesn't share a side with an opening, where it would stand in the way between rooms. With a single layer,
  it is on the start layer, but not on the start tile. If no tile fits, the level is planned again, like when
  the holes don't fit. In debug levels, it is at a random place away from the start, the ramps and the
  obstacles that reach its height. The text plan shows it as `E`.
- **Shape and trigger**: a 0.5 m cube whose bottom is 0.6 m above the floor. It belongs to the static
  `World`, but doesn't collide: the player walks into it. The level changes when the player's cylinder
  overlaps or touches the cube without rotation, also with noclip.
- **Next level**: its seed comes from the current one through a SplitMix64 step, so a series of levels
  repeats from the same first seed. Its layout is chosen at random as for the first level; `--layout`
  applies only to the first level. The player starts at the new start, standing still; noclip and the game
  time stay. The main loop builds a new renderer, with the meshes and texture of the new level.
  `next_level` does the same from the console.
- **Light**: a blue point light at the center of the cube, 5 m in range, whose brightness pulses between
  60% and 100% every 2 s. Only the brightness changes, so its shadow map, drawn once like a torch's, stays
  valid. It casts shadows and competes with the torch lights for the 31 places by distance.
- **Portal look**: the cube has its own vertex buffer, program and draw call, after the opaque world and
  the flames. Its vertex shader spins it once every 8 s around the vertical through its center. Its
  fragment shader paints each face as 8 × 8 big pixels: a bright frame around a turning, two-armed blue
  swirl with sparkles. It is unlit, so it glows in the dark, and pulses with the light. It is blended without
  writing depth, far faces first, then near faces. The darker bands of the swirl, and faces seen from the
  side, are partly see-through, so the far faces show behind the near ones. The cube casts no shadows.

Gains:

- **Easy to find**: the blue, pulsing light shows the way from a distance and around corners, and stands out
  against the orange torches.
- **No light needed on the cube**: a lit cube around its own light would face away from it and stay dark.
  An unlit shader avoids this, like the torch flames do.
- **Blending is easy here**: with one see-through object, drawn last, nothing needs sorting by depth. Back
  faces before front faces is a correct order for a convex shape, also with the camera inside it.
- **No asset**: the pattern is made in the shader, so it needs no texture file.
- **Reproducible**: the series of levels depends only on the first seed, and placement, the trigger, the
  light and the spin angle are plain code in `manifold_core`, with unit tests.

Drawbacks:

- **No reachability check**: in room levels, the exit should be reachable by how they are built (the rooms
  of a layer are connected, and neighboring layers have holes and, with the default settings, ramps), but
  nothing checks it. In debug levels, obstacles may enclose the exit. It may also be close to the start, e.g.
  right below it.
- **Failed generation quits the game**: when the next level can't be generated, the game stops with the
  error, see "Error-proof level generation" in [ROADMAP.md](ROADMAP.md).
- **The trigger doesn't spin**: it is checked for the cube without rotation, so a corner of the spinning
  cube can reach up to about 10 cm past it.
- **A new renderer per level** compiles all shaders again and drops all shadow maps. This is a short pause
  at the level change, which is fine as long as levels change rarely.
- **Blending works only for this one object**: more see-through things (water, glass) will need sorting, or
  order-independent transparency.
- Alternatives: a lit or emissive cube in the world mesh (the first two steps of this change: simple, but it
  can't move or be see-through), or a sprite that faces the camera like the flames (cheap, but flat when
  the player walks around it).

## 2026-11-15: Sliding doors in openings, activated with E by a ray from the camera

Status: **ACCEPTED**. Done in v0.2. The halves cast shadows since a later change.

Decision: openings between rooms get doors whose two halves slide sideways into the walls. The player opens and
closes them with E, which acts on what the center of the view points at.

- **Placement**: in room levels, each opening gets a door with the same chance, chosen for the level uniformly from
  10% to 100%.
- **Frames**: two near-black blocks in the opening, on its left and right, are plain static obstacles of the world.
  They collide, are part of the world mesh and cast shadows like walls. The gap between them (1.6 m with 2 m tiles)
  leaves room for the player (1.2 m wide).
- **Door**: two halves with a door texture, made on the CPU as an image, slide into the walls next to the opening,
  behind the frames, within one second. All doors of a level have the same look, chosen at random with a chance of 50%
  each: wood (planks with an iron band) or metal (brushed steel plates with a riveted rib), repeated up the door. The
  halves are opaque, so that closed doors can later block light and monsters.
- **State**: the shape of a door belongs to the static `World`. How far it is open and whether it moves live in `Game`,
  like the player.
- **Collision**: a door that is not fully open is one box between the frames, as thick as the door. It is passed to
  `move_and_slide()` next to the world. Passing is possible only when the door is fully open. A door doesn't close
  while the player overlaps the box.
- **Activation**: E casts a ray from the eyes along the view, up to 2 m. The ray stops at walls, slabs and closed
  doors. A hit on a closed door opens it, and a hit on a frame opens or closes its door. While a door moves, E on it
  does nothing. The ray test and its result are meant to be reused by other things that can be activated.
- **Drawing**: the halves have their own vertex buffer in the world's vertex format, drawn with the world's program
  and the door texture instead of the level texture. So they are lit and cut away like the world. The buffer is
  built again on the CPU when a door moves.
- **Shadows**: at first, the halves cast no shadows, like the exit. Now they are drawn into the shadow maps too.
  When a door moves, the maps of the lights whose range reaches anywhere its halves can slide are drawn again in
  that frame, also maps that are kept for lights not drawn now. A torch's map is 256 pixels square and its range
  6 m (9 m since v0.3, see the entry on the torch range), so the few maps near a door are redrawn every frame for the
  one second the door moves.
- `Obstacle` gets a rectangular footprint and a choice of surface (textured or dark), which the frames and the
  door boxes need.

Gains:

- Frames and collision reuse what exists: obstacles and `move_and_slide()`. Only the ray test and the moving mesh are
  new.
- No new shader and no new asset file: the door texture is an image made in code, bound in place of the level texture.
- Everything except drawing is in `manifold_core` and can be tested.

Drawbacks:

- While a door moves, the shadow maps of the lights near it are drawn in every frame, each with all 6 cube faces and
  the whole level mesh. Next to a door with a torch 2 m away, this took about 3 ms more per frame. If it gets too
  slow, only the cube faces that can see the door could be drawn again, which is usually 1 to 3 of them.
- The ray test checks every obstacle of the level, which is fine once per key press, but not every frame.
- Closing works only by aiming at a frame, since an open door's halves are hidden in the walls.
- Alternative: hinged doors that swing. They look more natural, but need rotated, moving collision, which can push
  the player, and swing into the room.

## 2026-11-17: Footsteps by distance walked, with sounds chosen by the directory of the level texture

Status: **ACCEPTED**. Done in v0.3. Since running was added (see below), walking is 2.5 m/s, a footstep is 1 m
walking and 1.5 m running, and the first one comes a quarter of a step after starting from standing. Since the asset
mapping (see below), the kind is the key of the texture in `assets/asset_mapping.json` (`LEVEL_TEXTURES_GRASS`, ...)
rather than its directory, and the footsteps are the files of `FOOTSTEP_SOUNDS_GRASS`, ...; level generation chooses
the texture, and `Game::set_texture_kind()` is gone. A file must be listed in the mapping, wherever it is, rather than
put in a directory, and a kind without textures in the mapping is left out of the choice when levels are generated.

Decision:

- **Surface**: each level has a `TextureKind` (grass, wood, metal or stone), the kind of its texture. The kind is the
  directory the texture is in: `assets/textures/grass/`, `wood/`, `metal/` or `stone/`, with any subdirectories.
  Stone covers everything else that is hard underfoot, such as bricks, tiles, plaster and dirt. PNG files elsewhere
  under `assets/textures/` are not used. The texture is still picked from all files at once, in the same sorted
  order as before, so each file has the same chance as before and a seed gives the same texture. The main loop picks
  the texture and sets the kind with `Game::set_texture_kind()`. The plain grey fallback counts as stone.
- **Sounds**: `assets/sounds/footsteps/<kind>/` has single steps, at least one per kind. Each step is cut from a
  recording of several steps by `scripts/cut_sounds.py`, which also fades its ends and scales it to a peak of
  -6 dBFS. The README next to the files gives the exact command, with the original recording and the times of the
  steps in it.
- **When**: `Game` counts footsteps. A player on the ground makes one per 1 m walked, which is 3 per second at the
  walking speed of 3 m/s. The first one comes 0.25 m after starting from standing, so that short moves are heard too.
  Landing faster than 2 m/s, after a fall of more than about 0.2 m, makes one too. Noclip and falling make none.
  `Game::take_footsteps()` gives the count since the last call, like `take_door_moves()`.
- **Playing**: the main loop plays a random step of the level's kind for each footstep, never the same one twice in a
  row, at half volume. `AudioOutput` mixes up to 4 sounds, so steps don't cut off other sounds.
- **Missing files**: a kind without textures or without footstep sounds only logs a warning when the game starts. Its
  textures are left out of the choice, or its steps are silent.

Gains:

- Adding a texture or a step sound needs no code: putting the file in the right directory is enough. Adding a kind
  needs one enum value and its two directories.
- The pace of the steps follows the actual movement: walking into a wall or standing makes no steps, and a different
  walking speed or frame rate keeps 1 step per metre.
- The counting is in `manifold_core` and is unit tested. With the `walk` console command, scripted runs can make
  footsteps, which can be checked with SDL's disk audio driver.
- Single steps in random order sound less repetitive than a looped recording, and they start and stop with the
  player.

Drawbacks:

- The whole level has one kind: all floors, ramps and slabs sound the same, since they share one texture.
- The kind comes from where a file is, not from what it shows. A texture in the wrong directory gets the wrong steps.
- Steps don't depend on how far away or in which direction they are, which is right for the player's own steps but
  not for others' later.
- A few recordings with a few steps each (5 to 10) still repeat noticeably over a long walk.
- Alternatives:
  - **Loop the whole recording while walking.** Simpler, but its rhythm doesn't match the speed, and stopping cuts a
    step in half.
  - **Play steps on a timer while a movement key is held.** No distance needed, but walking into a wall or on the spot
    would make steps.
  - **A kind for each surface of the level**, e.g. from the tile under the player. More realistic, once levels have
    several textures. It would replace the level's kind with a lookup of the surface under the player's feet.
  - **Speed up the recordings instead of cutting them** (the first try: 1.5x and 2x). It changes only the pace of the
    recording, which the game sets itself now.

## 2026-11-17: Save games as plain JSON in the user's data directory

Status: **ACCEPTED**. Done in v0.3. Since v0.4, saves are also made in the middle of a level, in a second file, see
"Save the middle of a level too" below; what is said here of saves made only at the start of a level holds for
`save_level_start.json`.

Decision: a save is a small JSON file, `save.json`, written with nlohmann/json to the directory that
`SDL_GetPrefPath()` gives: `$XDG_DATA_HOME/manifold/` on Linux, by default `~/.local/share/manifold/`. It holds only
what is needed to generate the saved level again (seed, level number, game time at the level's start, a layout given on the command line)
and a format version.

Gains:

- Saves are made only at the start of a level, so they are tiny. Plain JSON can be read and edited by hand, which
  helps to debug, and nlohmann/json is a header-only library that CMake downloads.
- The XDG data directory is where Linux games keep saves (`~/.config` is for settings, `~/.cache` for files that
  may be deleted). SDL3 gives the right directory on Windows and macOS too.
- Writing a temporary file and renaming it keeps the old save if the game stops while saving.

Rejected alternatives:

- **Zipped JSON** (renamed to `.sav`): needs a zip library, saves nothing for a file of a hundred bytes, and only
  hides the contents, which doesn't matter for a single-player game.
- **YAML**: no simpler to read for files this small, while its libraries (yaml-cpp) are larger and its syntax has
  more pitfalls.
- **A binary format**: can't be checked by hand, and needs care for byte order and versions.

Drawbacks:

- Levels are not stored but generated again, so a save only gives the same level with the same level generation.
  The version number must be increased when generation from a seed changes, which refuses older saves.
- When levels get state that changes during play (monsters, items), it must be added to the save, but only the state
  at the start of a level, since saves are made only then.

## 2026-11-18: Build the game's interface from own controls on the overlay renderer

Status: **ACCEPTED**. Done in v0.3. Since the inventory, the interface is scaled by the picture's height over 300,
also by fractions, so that it looks the same at every size; the console is still scaled by whole pixels.

Decision: the game's menus, and later the party screen with tabs for stats, level-ups and inventory, are built from a
small set of own controls in `manifold_core` (`src/ui/`), drawn as an `OverlayMesh` with the blocky font of the
console. Open screens form a stack (`ScreenStack`): only the top one gets input, all are drawn, and the game is paused
while any is open. A `Screen` keeps its state (which control has the focus) but no layout: it lays itself out for each
event and each drawing from the size of the picture, so that the mouse always hits what is shown, also in screenshots
of other sizes. Input reaches screens as device-independent events (`NavKey`, `PointerEvent`), so every control is
used with the keys and with the mouse. Escape opens the game menu, and Ctrl+Q quits at once.

Gains:

- No new library: the menus look like the console, with the same retro font, and the colors are shared in
  `ui_style.hpp`, so later screens look the same.
- Everything except drawing the mesh is in `manifold_core` and unit tested: layout, focus, clicks, which item does
  what.
- New screens are new `Screen` classes made of the same controls (`ButtonList` now, later tabs, labels, lists), and a
  dialog opened from a menu is pushed onto the stack above it.
- Ctrl+Q is the usual quit shortcut of Linux applications, and hard to press by accident.

Rejected alternatives:

- **Dear ImGui with a custom style**: it already draws the debug overlay and has tabs, tables and drag and drop, but
  it looks like a tool unless it is restyled heavily, uses another font, and can't be tested without a window.
- **RmlUi** (HTML and CSS for game interfaces): good for large screens such as an inventory, but it needs FreeType and
  a render backend, which is a lot for a few menus. It can still be adopted if the own controls grow too complex.
- **Shift+Escape to quit**: easy to remember next to Escape, but unusual elsewhere.

Drawbacks:

- Every control must be written by hand: tabs, scrolling lists and drag and drop for the inventory are still to come.
- The font has only ASCII characters and one size, scaled by whole pixels (since the inventory, by the picture's
  height over 300, so also by fractions, at which it looks a little uneven).
- Layouts are computed for every event and drawing. This is cheap for a few controls, but large screens may need to
  cache them.

## 2026-11-19: A party of three characters with random portraits, kept in the save

Status: **ACCEPTED**. Done in v0.3. Since the asset mapping (see below), the portraits are the files of the keys
`PORTRAITS_MALE`, `PORTRAITS_FEMALE` and `PORTRAITS_ANY` of `assets/asset_mapping.json`, and the save holds each
portrait as its key and index instead of its path, so a remapping changes the portraits of saved parties.

Decision: the player is a party of three characters that moves as one body: the player's cylinder, as before. A new
game chooses each character's portrait at random, without repeats, from the game's seed, among those in
`assets/portraits/`, whose directories (`male/`, `female/`, `any/`) give the character's gender; for `any/` the
gender is random. Each character has its own hit points. The party is stored in the save, as it was at the start of the level,
instead of being made again from the seed when the game is loaded.

Gains:

- A party gives the feel of classic party role-playing games, with room for each character to have its own
  stats, spells and portrait, while movement and collision stay as simple as for one body.
- A seed gives the same party, as it gives the same level, so screenshots and bugs can be reproduced.
- Keeping the party in the save keeps it unchanged when portraits are added or removed, and keeps what changes during
  play, such as hit points, and later experience and items.
- The gender stored with each character lets later features (names, voices, texts) depend on it without looking at the
  portrait's directory again.

Rejected alternatives:

- **One character**: simpler, but the roadmap plans a party with shared mana, and portraits are a large part of the
  look of the game.
- **Four or six characters**: more to manage and to draw for each one, before there is anything that tells them apart.
- **Making the party again from the seed when loading**: a smaller save, but the party would change with the portraits,
  and its state during play would still have to be saved.

Drawbacks:

- Old saves (format version 1) are refused, so a player loses a saved game when updating.
- With few portraits, some characters have none, and with many, players see only a few of them per game.
- The characters can't be told apart in the world, since they move as one body.

## 2026-11-19: Make random choices with own helpers on the raw output of the engine

Status: **ACCEPTED**. Done in v0.3.

Decision: every random choice that a seed decides (level generation, the level's texture, sounds and ambient light,
the party) is made with small helpers in `src/util/random.hpp` (`random_index`, `random_int`, `random_float`,
`random_chance`, `random_weighted_index`, `random_shuffle`), which use only the raw output of `std::mt19937_64`. The
standard distributions, `std::shuffle` and `std::sample` are not used, and a test (`PortableRandomChoices`) checks the
sources for them.

Gains:

- The C++ standard fixes the output of `std::mt19937_64` for each seed, but not the algorithms of the distributions
  and of `std::shuffle`, which differ between libstdc++, libc++ and MSVC's library. With the helpers, the random
  choices of a seed are the same on Linux, macOS and Windows, which is needed for saves, which store only the seed, to
  move between platforms, and for seeds in bug reports and PRs to show the same level everywhere.
- The helpers are unbiased (`random_index` draws again instead of taking a biased modulo), and their results for a seed
  are pinned by a test, so a change that would change all levels can't happen unnoticed.
- One place explains why, instead of a comment at each use.

Rejected alternatives:

- **The standard distributions everywhere**, accepting that a seed gives other levels on other platforms: the least
  code, but saves and seeds wouldn't be portable once Windows and macOS are supported.
- **Own helpers only where convenient** (the state before): the texture, sounds and party were portable, but the level
  itself wasn't, so the portable parts gained nothing.
- **A third-party library** of portable distributions (such as Boost.Random): more than the few helpers needed.

Drawbacks:

- Changing to the helpers changed the levels of all seeds, so saves of the earlier format are refused.
- Each new kind of random choice needs a helper of its own, with tests.
- It is necessary but not enough for the same levels everywhere: level generation also uses math functions such as
  `std::cos`, whose last bits can differ between C libraries, see "The same levels on every platform" in
  [ROADMAP.md](ROADMAP.md).
- The check for banned functions is textual, so it fails on a comment that names them as code would use them.

## 2026-11-19: Spells as timed effects that cost shared mana, starting with Candlelight and Levitate

Status: **ACCEPTED**. Done in v0.3.

Decision: the party has one pool of mana (100 of 100 at the start), which regains 1 per second of game time. A spell is
an effect that lasts 60 seconds of game time after it is cast and costs mana when it is cast. Casting a spell that is in
effect starts its duration again instead of adding to what was left. A cast without enough mana does nothing, without a
message for now. The first spells are **Candlelight** (cost 5), the party's light at the eyes, which was always on
before, and **Levitate** (cost 10), which turns off gravity and lets the party fly up and down with collision, unlike
noclip. Each spell has its own key for now (C and L), and loading moved from L to F9. Durations and mana are floats in
seconds and mana units. The mana and the spells in effect, with the time left of each, are kept in the save, as they
were at the start of the level.

Gains:

- Light becomes a resource: levels are darker by default, so torches and the portal matter more, which fits the goal of
  believable lighting in dark levels.
- Levitation reuses the movement and collision code: it is the noclip path with move_and_slide() instead of a free
  move, without gravity, footsteps or sticking to ramps.
- Starting the duration again keeps a spell's effect easy to predict, and spamming a key can't stack up hours of it.
- Floats in seconds follow the convention of the game time and the frame duration, and make mana regain smoothly
  rather than in steps of whole points.
- Shared mana is simpler than mana per character while the characters can't be told apart.

Rejected alternatives:

- **Adding the duration of a new cast to what is left**: lets players stock up on effects, and makes mana worth less.
- **Toggled spells that drain mana while on**: closer to a switch than a spell, and harder to show and balance.
- **Mana per character**: needs choosing who casts, before there is anything that tells the characters apart.
- **Durations in whole frames or milliseconds**: exact, but the game advances by float seconds anyway.
- **Keeping L for loading and casting Levitate with another key**: L is easier to remember for Levitate, and loading is
  rare and also in the game menu.

Drawbacks:

- Saves of format version 3 are refused, so a player loses a saved game when updating.
- A key for each spell doesn't scale to many spells; a spell book or a quick bar will be needed.
- A failed cast gives no feedback, and the screen doesn't show how long a spell has left.
- Without Candlelight, levels without ambient light are nearly black away from torches, which can be confusing at the
  start of a game.

## 2026-11-19: Torch range of 9 m and a torch next to the start of the first level

Status: **ACCEPTED**. Done in v0.3.

Decision: torch lights reach 9 m instead of 6 m. The first level of a game also gets an extra torch on the wall side of
the start room nearest to the player start that has no torch, whether or not the room has torches or the level has
ambient light. This replaces the torch on a random wall of the start room that only dark first levels without a torch
in that room got. Among equally near sides, one is chosen from the level seed with its own salt, so the rest of the
level stays the same.

Gains:

- A new game starts without Candlelight, and the first room is now always lit near the party, which shows where it is.
- With the longer range, rooms are lit further from their torches, so levels are less confusingly dark.
- Levels of all seeds are otherwise unchanged, and saves stay valid.

Rejected alternatives:

- **12 m**: more light, but each torch reaches about 4 times the area of 6 m, so many more torches reach a moving door,
  whose lights' shadow maps are drawn again in every frame (see the entry on doors), and more of them compete for the
  31 places of the lights drawn.
- **Keeping 6 m and making torches brighter**: brightens the rooms near the torches, but leaves their far parts dark.
- **The extra torch only for dark first levels, or on a random wall**: as before; the first room could still be dark
  where the party starts.

Drawbacks:

- More shadow maps are drawn while a door moves, which may cause stutter near many torches; limiting them is planned
  (see Fewer shadow maps per frame in ROADMAP.md).
- Lights fade out at the cut of the lights drawn more often.
- The extra torch can hang 1 m beside the start, which is bright up close.

## 2026-11-21: Small chests as objects of their own, placed at walls

Status: **ACCEPTED**. Done in v0.4. Since the asset mapping (see below), activating a chest plays its sound, and
ray casts hit chests. Since the inventory (see below), chests hold items, which activating a chest shows.

Decision: a small chest is an object of its own in the world (`World::chests()`), with a position on the floor, the
direction it faces and its look, not a decoration of a wall or of a floor tile. Level generation puts it at a wall, 5 cm
from it, facing into the room. In room levels, each room at least 2 x 2 tiles, except the start room, gets one with a
chance of 20%, at a random wall side of a tile that isn't part of a hole or a ramp, isn't next to an opening and isn't
the exit's tile. 20% of chests are golden, the others wooden. For now chests don't collide and do nothing when
activated. They are placed after all other random choices of a level.

Gains:

- A chest stays where it is if its wall is ever removed, and it can later get its own state (contents, open or
  closed), like doors have DoorState.
- The same kind of object serves room levels and the debug layout, which has no tiles.
- Levels of all seeds are otherwise unchanged, and saves stay valid.

Rejected alternatives:

- **Chests as wall decorations, like torches**: a chest stands on the floor, not on the wall, so it would have to be
  moved or dropped when its wall goes.
- **Chests as floor tile decorations**: ties chests to the tile grid, which the debug layout doesn't have, and to one
  place per tile.
- **Chests in corridors and next to openings**: they would stand in the way once chests collide.

Drawbacks:

- Without collision, the player walks through chests.
- A chest right below a torch gets little light on its front, which faces away from the torch.
- Golden chests look shiny only by their color: the renderer has no specular highlights.

## 2026-11-23: Full screen at the desktop's resolution, set in layered settings files

Status: **ACCEPTED**. Done in v0.4.

Decision: the game reads its settings from JSON files: its own `settings.json` (in the root of the repo, next to the
executable in a copy of the game), then the player's `settings.json` in the directory of the save, whose fields replace
those of the game's file. The first setting is `fullscreen`, which the game's file sets to `true`. Full screen is
SDL's borderless mode at the desktop's resolution: the screen's mode isn't changed, and the 16:9 picture is scaled to
the screen with black bars, as in a window. Runs with `--exec` or `--screenshot` always open a window. There is no key
to switch while playing; in-game settings come later.

Gains:

- No mode switch: the game starts and quits without the screen going black, other windows keep their places, and the
  picture uses the screen's own pixels, so it isn't blurred by scaling in the monitor.
- The player changes settings in their own file, which a new copy of the game doesn't replace, while the game's file
  gives the defaults and can get new settings in later versions.
- Scripted test runs, as used to check rendering, don't take over the screen, and their screenshots keep the size of
  the window.

Rejected alternatives:

- **Exclusive full screen with a mode change**, e.g. to a 16:9 mode without black bars: slow to switch, leaves
  other windows misplaced on some desktops, and the modes on offer differ between screens.
- **A command-line option** such as `--fullscreen`: has to be given on every start, and doesn't suit settings that
  the game menu will change later.
- **One settings file, copied to the user's directory on the first start**: new settings of later versions wouldn't
  reach a player who already has the file.

Drawbacks:

- The picture is drawn at the screen's full resolution, which is slower than a 1600 x 900 window on large screens.
- Settings are changed only by editing a file until the game menu gets a settings screen.

## 2026-11-23: Running with Shift at 4.5 m/s, and walking at 2.5 m/s

Status: **ACCEPTED**. Done in v0.4.

Decision: holding Shift makes the player run. Walking slows from 3 m/s to 2.5 m/s, a brisk walk, and running is
4.5 m/s, a fast run (3:42 minutes per km). Only the forward part of the movement, in the horizontal direction of the
view, gets the running speed: walking backward and stepping aside stay at walking speed, and running diagonally is
faster only in its forward part. If a frame of running is pushed back sideways by a wall, a block or a door by more
than half of its horizontal movement (sin 30°), the frame is moved again at walking speed. So running into something at
more than 30° from along it, also sliding along it, is no faster than walking, while at a shallower angle the player
slides along it at running speed. Ramps push the player straight up, so running up them stays fast. Running works on
the ground and with noclip, but not with Levitate. A running footstep is 1.5 m instead of 1 m. Running costs nothing
yet; stamina comes later.

In the air, after a jump or a step off a ledge, without noclip or Levitate, the player keeps the horizontal velocity
of the take-off, the frame that left the ground and was still moved by the keys. The keys change nothing until the
player lands, and turning only turns the view. The velocity is that of the actual movement of each frame, so what
stops the player in the air, such as a wall, also removes that part of it. Teleporting, noclip and Levitate leave the
player at rest, so a fall after them is straight down.

Gains:

- Speeds near real ones: 3 m/s was a jog, with 180 steps per minute. Now a walk is about 150 steps per minute and a
  run 180, at step lengths of 1 m and 1.5 m.
- Running only forward keeps strafing and backing away slow, as they are for a real runner.
- Slowing down at walls keeps running from making the player shoot along walls and into corners, and the check is
  cheap: a second collision pass only in frames that hit something steeply.
- Momentum in the air is realistic: a running jump goes farther than a walking one, and a jump can't be steered or
  stopped in the air. It is simple, since only the take-off velocity is kept, and the velocity isn't used on the ground.
- The 30° limit lets the player run along a corridor without holding a heading exactly parallel to its walls, which
  the keys can't do. The check compares the push with the frame's movement, so it doesn't depend on the frame rate.

Rejected alternatives:

- **Twice the walking speed of 3 m/s, i.e. 6 m/s**: faster than the pace of elite marathon runners, which doesn't
  fit the game's realistic speeds.
- **A realistic slow walk of 1.4 m/s**: too slow to cross the levels, which are tens of metres wide.
- **Running in every direction**: runners don't run backwards or sideways at full speed.
- **Clamping the speed of the slide along a wall** instead of moving the frame again: needs changes in the collision
  code, while moving again at walking speed gives the same slide as walking.
- **Control in the air**, as before: a jump could be steered, stopped or sped up with Shift in mid-air, which a real
  jump can't.
- **Momentum on the ground too**, with acceleration and friction: smoother starts and stops, but the player would
  slide past doors and ledges, and it needs tuning.
- **Slowing down at any touch of an obstacle**: running along a wall at a heading even a little into it, as when
  hugging a corridor wall, would be at walking speed for as long as the wall is touched.

Drawbacks:

- Walking is a sixth slower than before.
- On the ground there is no momentum: running stops at once when Shift is let go. In the air, a jump can't be
  corrected, so a missed jump onto a ledge can't be saved.
- Running into a wall at about 30° switches between walking and running speed as the angle crosses the limit, and
  a frame that hits a corner steeply moves at walking speed, which can feel like a small stutter.

## 2026-11-24: Mouse look toggled with M, off by default, paused while the pointer is needed

Status: **ACCEPTED**. Done in v0.4.

Decision: the mouse can turn the view, as the arrow keys do, within the same 40° of pitch. M turns mouse look on and
off while playing, and the `mouse_look` setting, `false` in the game's `settings.json`, says how the game starts; M
doesn't write the setting. `mouse_sensitivity` gives degrees per unit of mouse motion, 0.1 by default. While mouse look
is on, SDL's relative mouse mode hides the pointer and keeps it in the window, and a left click activates what the
center of the view points at, as E does. Menus, the console and the debug overlay pause mouse look, since they are
used with the pointer, and it goes on again when they close. Runs with `--exec` or `--screenshot` start with it off.

Gains:

- Turning and looking with the mouse is faster and finer than with the arrow keys, as players of first-person games
  expect, while the keys still work, also together with the mouse.
- With it off, as by default, the pointer stays free for activating what it points at anywhere in the view, as before.
- A click with mouse look on does what it would do in most first-person games, at the crosshair, rather than nothing.
- Pausing rather than turning it off means a quick look at the debug overlay or a menu doesn't need M again.
- Scripted runs don't take the mouse from the desktop, as they don't take the screen.

Rejected alternatives:

- **Mouse look always on**, or holding a button to look: always on would lose clicking anywhere in the view, and
  holding a button is tiring for long walks.
- **Ignoring clicks with mouse look on**, so that only E activates: a click is the usual way to activate at the
  crosshair, and nothing would be lost by it, since with a hidden pointer a click can't aim elsewhere.
- **Turning mouse look off when the debug overlay opens**: the player would have to press M after every look at it.
- **M writing the player's `settings.json`**: settings are written only once the game menu has a settings screen.
- **A sensitivity scaled by the frame time or smoothed**: a motion of the mouse should turn the view by the same angle
  at any frame rate, without lag.

Drawbacks:

- The sensitivity is in the mouse's own units, so the same setting turns more with a mouse of a higher resolution,
  and there is no inverted look yet.
- The mouse look key is the key at the place of M on US keyboards, which is labelled otherwise on some layouts, as are
  the other game keys.

## 2026-11-25: Assets referred to by keys of a mapping file, and by indices in game objects

Status: **ACCEPTED**. Done in v0.4. Since the inventory (see below), the mapping also has `ITEM_ICONS`, an object
from the keys of items to their icons, which aren't keys of `AssetKey`, so that items are added without code. Since
the casting sounds, it also has `SPELL_SOUNDS`, an object from the keys of spells to their casting sounds.

Decision: `assets/asset_mapping.json` says which files the game uses for what. Each of its keys, such as
`DOOR_OPEN_SOUNDS`, `CHEST_OPEN_SOUNDS`, `LEVEL_TEXTURES_GRASS` or `PORTRAITS_MALE`, has a path or a list of paths,
relative to `assets/`, and one file may serve several keys: chests use the files of door opening for now. The keys and
the type of file each needs (OGG sound, PNG texture of at most 128 × 128, PNG portrait) are fixed in code
(`AssetKey`). The file lists every file explicitly, not directories. Game objects keep the index of their asset among
the files of its key (`AssetIndex`), not a file name: doors, chests and exits their sounds, levels their texture, and
characters their portrait, also in saves. Level generation chooses each index uniformly among the files that the key
has (`choose_asset()`), with random numbers of its own, so the rest of a level doesn't depend on the assets. A file
that is missing, can't be read or isn't of the right type keeps its index but gives no sound, the plain grey texture or
an empty portrait, after a warning; so does an index beyond the files of its key, e.g. in a save made with another
mapping. A test checks that every key of the game's mapping has files, that each can be used, and that every OGG and
PNG file under `assets/` is mapped.

Gains:

- Assets can be remapped, and new files added, without building the game again, also in a copy of the game.
- One file can serve several purposes, such as doors and chests, without copies or a directory tree that spells out
  every use.
- Indices stay valid when files are moved or renamed, and are small and stable to store in saves, which mid-level
  saves and asset choices made after level generation will need.
- A broken or missing file costs one asset, with a warning, instead of stopping the game or shifting the others.

Rejected alternatives:

- **Directories as the mapping** (the state before): each use needs a directory of its own, so a file used twice
  must be copied, and renaming a directory needs a change of code.
- **Directories as values in the mapping**: shorter, but which files and in which order an index means would depend
  on what is on disk; explicit files make each index mean one file.
- **A mapping file for each type of asset** (sounds, textures, portraits): three files to keep in sync, and a key
  could be put in the wrong one.
- **File names in game objects and saves**: break when a file moves, and tie objects to the file tree.
- **Raw random numbers taken modulo the number of files** (as doors had): level generation wouldn't depend on the
  mapping, but an index wouldn't name a file, and a remapping would silently change every choice.

Drawbacks:

- The mapping must be edited to add a file: a new file is unused until it is listed. The test of the game's mapping
  names files that are listed nowhere.
- How many files a key has decides the random choices, so a remapping that adds or removes files changes the assets
  that seeds give and the portraits of new parties.
- The order of the files decides what a saved index shows: a save shows another portrait if portraits are inserted,
  removed or reordered before its own, and none if the key has fewer files than its index.
- The mapping is long: about 240 files, most of them textures of stone.
- Old saves (format version 4) are refused, since they store portraits as paths.

## 2026-11-25: Items chosen for chests by number and value, in one inventory of the party without stacking

Status: **ACCEPTED**. Done in v0.4. The limits of chests and the chance of golden chests moved to
`data/constants.json` on 2026-11-27, see "Game settings in strict JSON files of `data/`". Items of a kind are stacked
in one cell since 2026-11-28, and saved as stacks of keys with numbers, see "Items of a kind stacked in one cell".

Decision: the kinds of items are listed in `data/items.json`, each with a key, a name, a value, a weight in grams and a
chance; their icons are in `ITEM_ICONS` of the asset mapping, by the same keys. `data/` is a new directory, found like
`assets/`, for files that define the game rather than how it looks and sounds; later it will hold level settings and
other constants. When a level is generated, each chest gets a number of draws, uniformly from 1 to 3 for a wooden chest
and from 3 to 5 for a golden one. Each draw chooses an item with a probability proportional to its chance among the
items worth 100 to 2000 (wooden) or 100 to 5000 (golden). A drawn item that would make the chest's items worth more than
2000 (wooden) or 6000 (golden) together is left out, and the filling stops there, so chests often get fewer items than
draws: in a simulation, wooden chests got 1, 2 or 3 items 39%, 46% and 15% of the time, and golden chests 1 to 5 items
14%, 28%, 35%, 17% and 6% of the time. These limits are constants in code for now (`ContainerLoot`). Values are never
shown to the player. The party shares one inventory, a list of items without a limit of weight, which is saved by the
keys of the items at the start of each level; what chests hold isn't saved, since a loaded level starts again. The
inventory is a scrollable grid of cells, one item per cell. An item is moved by selecting it with a click and clicking
an empty cell, which puts it at the end of that grid's list; with a chest open, the two grids are side by side, a double
click or Enter moves an item to the other side, and Take all moves everything. The first items are ten gems: a large
diamond is worth 5000, a ruby 4000, an emerald 3000, a sapphire 2500 and an amethyst 500, a small gem a quarter of its
large one, and all have the same chance.

Gains:

- The number of draws says how much a chest holds, and the range of values how rich it is, independently: a golden
  chest holds more items on average and finds the dear ones, a wooden chest never gets more than a few.
- A new item needs only its value and chance, and monsters and other containers can be filled the same way with their
  own limits.
- Chances tune how common an item is independently of its value, and they needn't add up to anything, so adding an
  item doesn't change the others' numbers.
- Items, values and icons change without building the game, and `data/` keeps the rules apart from the assets.
- Saving item keys keeps saves valid when items are added or reordered.
- One shared inventory is simple to use and to save, and fits a party that moves as one.

Rejected alternatives:

- **Filling up to a random total value** (the first version of this change: wooden 500 to 1500, golden 2000 to 6000,
  at most 8 items, each chosen among those that still fit): once the dear items no longer fit, only the cheapest are
  left, so chests filled up with amethysts.
- **Skipping an item that doesn't fit and drawing again**, or retrying a few times: chests would nearly always get
  their fewest items, but the number of draws would no longer bound the work, and after a dear first item the rest
  would mostly be the cheapest gems.
- **Loot tables per container** (fixed lists of items with probabilities): each container would need its own table,
  and every new item would mean editing all of them.
- **Realistic prices**, where an amethyst is worth about a hundredth of a diamond: amethysts would be worthless next to
  the other gems. Amethyst is valued as it was when it counted among the precious stones.
- **`items.json` in `assets/`**: it holds rules, such as values and chances, not files of assets. **`config/`** as the
  name of the new directory: it reads like the player's configuration, which `settings.json` is.
- **An inventory for each character**: needs moving items between characters before there is any reason to, such as
  equipment or a limit of weight per character.
- **Drag and drop**: more code for the same moves; selecting and clicking, and double clicking, also work without it.
- **Stacking items of a kind in one cell**: worth doing, but later, see ROADMAP.md. Done on 2026-11-28.

Drawbacks:

- Chests often get fewer items than draws, 42% of golden chests fewer than 3: e.g. a large diamond (5000) drawn first
  for a golden chest leaves room for at most 1000, so most second draws end the filling. Stopping instead of drawing
  again keeps the filling simple and bounded, and the number of draws an upper limit. A total below the largest value of
  an item would let a dear first draw leave a chest empty, so the game warns about such limits.
- The limits of containers are in code, not yet in `data/`.
- An item can only be put at the end of a grid; there is no way to put it into a cell in between.
- Without stacking, many small items fill the grid quickly, and it must be scrolled. Solved by stacking on
  2026-11-28.
- What a chest holds depends on the order and number of the items in the file, so changing them changes the chests of
  seeds, as remapping assets changes the assets of seeds.
- Items taken from chests during a level are lost when the level is loaded again, as everything else of the level is.

## 2026-11-27: Game settings in strict JSON files of `data/`, with defaults in code for tests

Status: **ACCEPTED**. Done in v0.4.

Decision: the settings that are worth tuning move from code into three files of `data/`, next to `items.json`:
`spells.json` (the name, cost and duration of each spell, by its key), `level_layouts.json` (all fields of
`RoomLayoutConfig` and `DebugLayoutConfig` and the probability of each layout) and `constants.json` (the player's
radius, height, speeds and jump height, the party's size, hit points, mana and regeneration, the weights of the flames
of torches, the chance of golden chests, the loot limits of both kinds of chests, and the lights of `LightSettings` with
the luminance of the levels' ambient light). The game reads them at the start with `load_game_data()` into the structs
it already had (`GameConfig`, `LevelConfig`, `MagicConfig`, `LightSettings`). The files are strict: every field must be
there, in its range, and no field may be unknown; the game also generates a level of each layout to check settings that
only fail together. If anything is wrong, the game doesn't start and logs every problem with its file and field. The
structs keep their defaults in code, which the unit tests use; the game and the movement simulation use the files.
Angles are in degrees per second in the files, unlike the radians of the code. The sizes of the HUD, the colors of the
interface and of objects, the step lengths of footsteps and the like stay in code. What a spell does stays in code, so
the file can't add spells. Saves don't record the settings: a save gives another level when the layouts or the chances
of the objects of levels change, and a spell in effect is cut to its new duration when it is loaded.

Gains:

- Speeds, chances, layouts and lights can be tuned by editing a file and restarting, without building the game.
- A misspelt or forgotten field is an error rather than a silent default, so a tuned value can't be lost unnoticed,
  and all problems of all files are listed at once.
- The trial levels catch most combinations that each field's range can't, such as tiles too narrow for the player, at
  the start instead of when a level is generated in the middle of a game.
- The defaults in code keep tests independent of tuning: changing a value in `data/` doesn't break them, except the
  test that the files load without problems or warnings.
- The movement simulation checks the movement with the values the game uses.

Rejected alternatives:

- **Lenient files like `items.json`**, where a wrong field is left out with a warning and the default in code is kept:
  a typo in a tuned value would go unnoticed, and the game would play with a value nobody chose.
- **No defaults in code**, so that the files are the only place of the values: every test would have to read the data
  files or build all settings, and tuning a value would change the results of tests.
- **A test that the files match the defaults in code**: it would keep the two copies equal, but tuning would then need
  a change of the code too, which is what the files should save.
- **A hash of the settings in saves**, to refuse saves made with other settings: tuning would throw away saves while
  a game is being tuned, and changing items or assets already gives other levels without it.
- **One file for everything**: the spells and the layouts are tuned separately and have their own structure, and
  smaller files are easier to read.
- **Also the HUD sizes and the colors of the interface and objects**: they are about how the game looks rather than
  how it plays, and are rarely tuned. The lights are in, since they decide how dark the levels are.

Drawbacks:

- The values are in two places, the files and the defaults in code, which can drift apart; the tests then check other
  values than the game plays with.
- The trial levels take about 80 ms at the start, and try only one seed: settings that fail for some seeds, such as too
  few attempts to plan a level, can still stop the game when such a level comes.
- A field added to a struct must also be added to the reader and to the file.
- Saved games give other levels after the layouts or the chances of objects change.

## 2026-11-27: Save the middle of a level too, every 10 s of play and on exit, besides its start

Status: **ACCEPTED**. Done in v0.4. Since 2026-11-28, items are saved as stacks, each with a key and a number,
see "Items of a kind stacked in one cell", in the same version 5, which still reads the keys of older saves.

Decision: the game keeps two saves in the user's data directory, in the same JSON format (version 5).
`save_level_start.json` is the save of the start of the level, as `save.json` was before, written when a level starts.
`save.json` holds the game as it is: written when a level starts, every `autosave_interval` seconds of game time (10 by
default, in `settings.json`, with `autosave` to turn it off) and when the game quits. It is the start of the level with
an optional `progress` object: the game time, the party, its magic and items, the player's position, view, velocity and
whether they stand, the state of each door (openness and target) and the item keys of each chest, by their index in the
level. Loading generates the level again from its seed, as before, and then puts the progress back; a progress whose
numbers of doors or chests differ from the level's is refused. The start menu offers Continue (`save.json`) and
Restart last level; the game menu has Load game (also F9) and Restart last level, and the console `load
[level_start]`. Loading the start of the level writes nothing: `save.json` is replaced by the next autosave, an
autosave interval later, or on exit, so until then, loading it undoes a restart chosen by mistake. Saves are written on
the main thread, each to a `.tmp` file that is then renamed over the old one; autosaves aren't logged, nor are those
that fail after a failed one.

Gains:

- Quitting and coming back continues where the player was, with what they did in the level, and a crash loses at most
  10 s of play.
- The format stays the same, and the progress is optional: old saves load as saves of the start of a level, and older
  versions of the game ignore the progress, so the version needn't change.
- A save of the start of the level is always there, so a level can be restarted, e.g. after a mistake that the last
  autosave already holds.
- Doors and chests are saved by their index, without ids of their own, since the level generated again from the same
  seed has them in the same order.
- Game time doesn't advance while a menu is open, so autosaves don't rewrite the same state while the game is paused.

Rejected alternatives:

- **Two formats, or a new version**: the progress only adds to what a save of the start of a level holds, so one format
  with an optional object reads both, and keeps the old saves.
- **Saving only the progress in `save.json`, without the start of its level**: loading it couldn't give the start of
  the level for the next `save_level_start.json` or Restart last level, unless both files were always read together.
- **Writing on a worker thread**: the file is about 10 to 40 KB in a rooms level (most of it the doors), which takes well
  under a millisecond to write, and a thread would need a copy of the state and care when the game quits while writing.
- **Timing autosaves by wall-clock time**: they would rewrite the same state while the game is paused.
- **`fsync` before the rename**: it can take tens of milliseconds on a slow disk, every 10 s on the main thread. The
  rename alone keeps the old save if the game stops while writing; only a power loss right after it can lose both.
- **Saving by hand (quick save)**: autosaves and the save on exit make it less needed; it can be added later.
- **Replacing `save.json` at once when the start of the level is loaded**: one misclick on Restart last level, right below
  Load game, would throw away the progress in the level for good.
- **A confirmation for Restart last level**: an extra step every time, while keeping `save.json` until the next autosave
  makes a mistake easy to undo with F9.

Drawbacks:

- The save of the middle of a level grows with the number of doors and chests, and with what later changes during play
  (monsters), which must be added to the progress.
- A save made by a game whose level generation changed in a way that keeps the numbers of doors and chests is loaded,
  but puts the doors, chests and player into another level, e.g. into a wall.
- Noclip and footstep timing aren't saved, so a player who flew with noclip falls after loading.
- After a restart, quitting before the next autosave still replaces `save.json`, since the game always saves on exit,
  so the progress before the restart is then lost.
- A restart no longer makes the two files agree at once: an unreadable `save.json` stays until the next autosave, so F9
  fails until then, and if writing `save_level_start.json` failed when a level started, F9 after a restart goes to the
  level of `save.json`, which may be another one.
- Without `fsync`, a power loss right after saving can leave an empty or old save, depending on the file system.

## 2026-11-28: Items of a kind stacked in one cell, split with a dialog when moved between inventories

Status: **ACCEPTED**. Done in v0.4.

Decision: an inventory holds stacks (`ItemStack`: the kind of item and how many, at least 1), at most one of each kind,
in a list that keeps their order, with a hash map from the kind to the place of its stack (`Inventory`). Adding items
joins the stack of their kind, or starts one at the end, in constant time; taking all items of a stack removes it, and
the stacks after it move up, which renumbers them in the map. A stack takes one cell of the grid, which shows its
number at the bottom right, in orange with a dark shadow over the icon, if it has more than one item; numbers above
9999 are shortened to thousands, millions or billions (`12k`), so that four characters fit. Moving a stack within its
inventory moves all of it to the end, as before. Moving it to the other inventory, with a chest open, by a click on an
empty cell there, a double click or Enter, first opens a dialog over the inventory screen (`ItemSplitDialog`) when the
stack has more than one item: it starts with all of them, and a slider, - and + buttons, Left, Right, Up, Down (by
ten) and the wheel change the number; Enter or Move moves them, Escape, Cancel or a press outside cancels. Take all
moves everything without asking. Saves store each stack as `{"item": KEY, "count": N}`, and read a plain key, as older
saves of version 5 have, as one item; stacks of the same kind in a save are joined when loaded. Items with properties,
such as enchantments, will stack only with equal ones: the key of the hash map and of `ItemStack` becomes the kind with
its properties instead of the `ItemId`.

Gains:

- Many items of a kind take one cell, so the grid rarely needs scrolling, and the number is seen at a glance.
- Adding an item and finding its stack take constant time, and a stack of a million items takes as little memory and
  as little room in a save as one item, which `give` now allows.
- The dialog starts with all items, so Enter, Enter moves a whole stack as Enter did before, and splitting needs only
  a few more keys or clicks.
- The dialog is part of the inventory screen rather than a screen of its own, so it needs no new way for a screen to
  open another or to pass a result back, and it is tested like the screen.
- Old saves still load, so the version needn't change.

Rejected alternatives:

- **A `std::map` or a sorted list by kind**: the order of the stacks would be that of the item file rather than the
  order in which the party got them, and moving a stack to the end, as a click on an empty cell does, would be
  impossible.
- **A plain list of stacks, searched for the stack of a kind**: simple, and fast enough for a few dozen stacks, but
  adding an item would take time linear in the number of stacks, and inventories of monsters and containers will grow.
- **Several stacks of a kind, or a largest stack size**: splitting within one inventory would then make sense, but two
  stacks of rubies in one grid are only clutter while items have no properties, and a limit would be arbitrary.
- **Splitting within one inventory, or in the party's inventory alone**: with one stack per kind, the split part would
  join the same stack again.
- **Starting the dialog with one item, or half**: moving a whole stack, the most common move, would need more steps.
- **Typing the number**: the screens get no text input yet; the slider reaches any number with one press, and the keys
  fine-tune it.
- **The number at the top left or the top right of the cell**: the bottom right is where most games put it, and the
  icons of gems leave it emptier.
- **A new save version**: it would refuse the old saves, though the new format only adds a way to write several items
  of a kind.

Drawbacks:

- Taking a whole stack, or moving one to the end, renumbers the stacks after it in the map, in time linear in their
  number, as erasing from the list already takes.
- The second press of a double click on an empty cell is ignored, so that it doesn't press what the dialog opened by
  the first press shows there; a quick click there that was meant for the dialog is lost too.
- Shortened numbers above 9999 are exact only on the line at the bottom of the screen and in the console.
- Saves written now can't be read by the game before this change.

## 2026-11-28: Fall damage by the energy of the landing, randomized for each character, and Game over

Status: **ACCEPTED**. Done in v0.4.

Decision: a landing hurts by its vertical speed v, not by the height fallen: up to `safe_speed` (8.5 m/s, a free fall of
3.7 m) not at all, and above it by 100 hit points times (v² - safe²) / (full² - safe²), with `full_damage_speed` 18 m/s
(16.5 m). 100 hit points are an ordinary person's, so the damage doesn't scale with `max_hp`, and a character with more
hit points survives more. Each living character gets that damage times its own random factor from 1 - `spread` to
1 + `spread` (0.3), rounded to whole hit points, from a generator seeded by the level's seed. A character at 0 is dead
and stays at 0; when all are, the Game over menu offers Restart last level, New game and Exit game, and the game isn't
saved while it shows. A landing that hurts flashes the picture red for 0.5 s of game time.

Gains:

- Close to real falls: people mostly walk away from 3 m, are badly hurt from 6 to 10 m, and rarely survive 15 m or
  more onto a hard floor. The thresholds are a little higher than real ones, so dropping down a hole between two layers
  of rooms (3.3 m) is free, even at a low frame rate, whose landing speed has up to a frame more of gravity, which keeps
  the room levels playable.
- The kinetic energy of the impact, which the square of the speed measures, is what hurts, so the same rule holds in
  levels with other gravity, and a running jump hurts no more than a standing one.
- The spread makes a fall near the limit a gamble without making small falls deadly, and a fall can kill one
  character and spare another.
- The constants are in `data/constants.json`, so they can be tuned without building the game.
- The random numbers of falls have their own generator, so they don't change levels, and the same seed and play give
  the same damage.

Rejected alternatives:

- **Damage by the height fallen**: simpler to explain, but the height would have to be tracked from the top of each
  fall, e.g. of a jump, and the same height lands slower under weaker gravity, so it would hurt too much there.
- **Damage linear in the speed above the threshold**: the jump from harmless to deadly would be steeper at low speeds
  and flatter at high ones than the energy gives.
- **A share of `max_hp` instead of absolute hit points**: stronger characters wouldn't survive more, and the "100 hit
  points are an ordinary person" scale would be lost.
- **The same damage for the whole party**: the party would always die together.
- **Saving the generator's state**: a loaded game would give the same damage for a fall as the game it was saved
  from, but the save format would change, for little gain.
- **Saving while the party is dead**: Exit game would leave a save whose Continue only shows Game over again.

Drawbacks:

- An autosave can be made in the middle of a deadly fall, so Continue after a Game over may end in another one; Restart
  last level is always offered for that.
- The landing speed is that at the end of the landing frame, so it is up to a frame of gravity faster than at the
  contact: 0.33 m/s at 30 fps. The safe speed leaves room for it.
- Only falls hurt so far, so the Game over menu is only reached by them or by the `hp` cheat.
- The Game over menu's Restart last level loads `save_level_start.json`, so in runs that don't save, such as those with
  `--exec`, it loads whatever save is there, as the other menus do.
