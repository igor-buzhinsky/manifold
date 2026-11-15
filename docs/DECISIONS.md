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
- **Speed**: Jolt's broad phase only tests shapes near the player. Our code tests every obstacle on each
  move, which is about 12–20k in room levels.
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
- Alternative: keep our own code and add a spatial grid for speed. This is cheaper now, but stairs, props,
  doors and water would each need more of our own physics later.

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

Status: **ACCEPTED**. Done in v0.2, without shadows of the halves.

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
- **Shadows**: at first, the halves cast no shadows, like the exit. Later they can be drawn into the shadow maps,
  whose maps are drawn again for lights that reach a moving door.
- `Obstacle` gets a rectangular footprint and a choice of surface (textured or dark), which the frames and the
  door boxes need.

Gains:

- Frames and collision reuse what exists: obstacles and `move_and_slide()`. Only the ray test and the moving mesh are
  new.
- No new shader and no new asset file: the door texture is an image made in code, bound in place of the level texture.
- Everything except drawing is in `manifold_core` and can be tested.

Drawbacks:

- Until the halves cast shadows, light from a torch goes through a closed door as through an open opening.
- The ray test checks every obstacle of the level, which is fine once per key press, but not every frame.
- Closing works only by aiming at a frame, since an open door's halves are hidden in the walls.
- Alternative: hinged doors that swing. They look more natural, but need rotated, moving collision, which can push
  the player, and swing into the room.
