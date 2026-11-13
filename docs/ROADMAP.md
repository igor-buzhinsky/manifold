# Manifold — Roadmap

*Manifold* is a working title. It is a hobby first-person fantasy game where the player is a mage.
It looks and feels like *Daggerfall*, but is structured as a linear series of
rogue-like, procedurally generated 3D levels. There is no story or lore yet.

## Goals and constraints

- Runs on Linux, with full-screen support.
- Low performance requirements, no fancy graphics: a low-fi, retro look.
- Textures and models come from free sources or are AI-generated.
- Believable lighting is a core feature: dark levels lit by the player's light spell
  and by multiple other light sources.
- Open and non-commercial, if it is ever released at all; it may stay a personal experiment.
- Personal goal: get more experience with modern C++.

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
| Debug UI | **Dear ImGui** | Planned | Live tuning of lights, generator parameters, and so on. |
| Navigation | **Recast/Detour** | Later | Navmeshes for monsters, including multi-layer levels. |
| Audio | SDL3 audio or **miniaudio** | Later | |

Rejected alternatives:

- **raylib**: the fastest start, but HDR and multi-light shadows would mean working around it.
- **Godot**: gives most features for free, but most code would be GDScript instead of C++.

### Architecture principles: keep 3D paths open

The core is fully 3D, so that water, levitation and arbitrary 3D objects can be added later without
rewrites.

- All positions, velocities and transforms are `vec3`. Nothing in the game logic assumes `(x, y)`.
- No grid or raycaster renderer. The renderer draws arbitrary triangle meshes.
- **Collision is our own code**, not Jolt. The player is a vertical cylinder that moves and slides against
  rotated boxes, ramps and the level bounds, in 3D. Gravity, jumping, landing, and sticking to ramps
  when walking down them work on top of this. This was simpler than Jolt for the shapes needed so far,
  and is good C++ practice. It tests every obstacle on each move, which is slow in large levels. Replacing
  it with Jolt Physics is considered, see [DECISIONS.md](DECISIONS.md).
- A level is a 3D data structure: obstacles, ramps, bounds, gravity and the player start (position and yaw). Later also
  lights and volumes (for example water). Generators may work on a 2D grid internally, but nothing
  downstream depends on that.
- Level generation is deterministic from a seed, so levels can be reproduced for debugging.
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

## v0.1 — Exploration prototype (current: 2026-11-11, commit 3d75613)

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

## v0.2 — Lighting and level structure

- ~~**OpenGL 4.1**: move the renderer from 3.3 core to 4.1 core, which is as far as macOS goes.~~ Done.
- ~~Replace libepoxy with glad, which helps Windows and macOS support.~~ Done.
- To consider (see [DECISIONS.md](DECISIONS.md)):
  - **OpenGL 4.5**: debug output through the logger, Direct State Access, shader storage buffers and
    compute shaders. This closes the macOS path unless we keep a 4.1 render path.
  - **Jolt Physics**: replace our collision code with Jolt, with the player as a `CharacterVirtual`. Keep
    the current behavior: sliding, ramps, gravity, jumping and noclip.
- **Console**: commands that inspect the game, for testing levels by hand and for scripted checks, see
  [GAME_MANUAL.md](../GAME_MANUAL.md). Done: typed into the in-game console window (the `/~ key), into
  the terminal the game was started from, or given with `--exec`; `--screenshot` is short for a script.
  Commands: `help`, `level_info` (its text depends on the layout), `camera`, `teleport`, `noclip` (no longer
  a key of its own), `cutaway` (leaves out everything above a height, to look into a layer from above),
  `arrow`, `lighting`, `shadows`, `screenshot` (of any size, drawn off screen), `wait` and `quit`. The
  console window shows the log and has an input line with history. Its parts are reusable for the game's
  interface later: `TextField` (a line of typed text), `OverlayMesh` (rectangles and text in screen pixels,
  with the blocky stb_easy_font in cells of equal width) and `OverlayRenderer`.
- **Level exit**: an exit placed in a room far from the start, for example on another layer. It must be
  reachable from the start. Reaching it generates the next level with a new seed, which is a minimal
  version of the linear series of levels.
- **Doors between rooms**: some or all openings between rooms get doors that the player can open and
  close. A closed door blocks movement (and later light and monsters). Doors are drawn as their own
  geometry in the opening. Shadow maps assume that nothing moves, so a door that opens or closes must
  make the renderer draw the shadow maps of nearby lights again.
- **Multiple point lights**: static light sources placed in the level (torches for now; braziers and glowing
  crystals are under "Later (unordered, low priority, might not be done)"), with color and flicker. With 4.1,
  the lights near the player (a few dozen) go to the shader in a uniform buffer, chosen on the CPU each frame.
  With 4.5, a shader storage buffer can hold all lights of the level, and a compute shader can choose the
  lights for each part of the screen. Done for torches: room levels get torches on random wall sides facing a
  room (5–10% of them, random for each level), debug levels on their outer walls. All torches of a level have
  one flame color: orange (50%), blue or green (25% each). A torch is a six-sided handle in the world mesh
  (untextured, with a per-vertex color) and a flame sprite: an upright quad facing the camera, painted by a
  shader as a pixelated, flickering teardrop. Each frame, the player's light and the 31 torches nearest to the
  player (by distance minus range) go to the shader in a uniform buffer. Torch lights flicker and fade out
  smoothly at 6 m. The player's light now also has a range (15 m), and every light treats surfaces closer than
  0.3 m as 0.3 m away, so the wall behind a flame doesn't turn white. Lights don't pop when they enter or
  leave the nearest 31: when lights are left out, the gap (distance minus range) of the first one left out is
  the cut, and chosen lights fade out over the last 2 m before it. A light that is replaced has faded out
  fully, and the one that takes its place comes in from zero. This needs no state between frames.
- **Shadows**: cubemap shadow maps for the nearest N lights. With 4.1, all of them fit in one cubemap
  array texture, and each light's six faces are drawn in separate passes. 4.5 adds little here, apart from
  debug output and shorter setup code.
  Done for torch lights, up to 31 at once. The player's light casts none, because it is at
  the camera, where its shadows can't be seen (and the player has no body yet). Torches and the world don't
  move, so each torch's shadow map is drawn once, when the torch becomes one of the nearest, and kept in
  one of 48 slots of a 256 × 256 depth cube map array (about 38 MB). The slot used longest ago is reused.
  Drawing all 31 maps on the first frame takes about 0.2 s. A map stores the distance to the faces that
  point away from the light (back faces), divided by the range, so lit surfaces never shadow themselves
  and a 5 mm bias is enough. For this, the shadow pass also draws faces that the visible mesh leaves out:
  a shell behind the one-sided floor, ceiling and boundary walls, and the hidden tops and bottoms of
  blocks and ramps. Lookups use hardware comparison with 2 × 2 linear filtering. Torch handles and the
  player cast no shadows (see "Later (unordered, low priority, might not be done)"). Next: a limit on how
  many shadow maps are drawn in one frame, so that many torches at once don't cause a stutter.
- Pixelation (of textures or the entire visible region as a whole).
- First static 3D props loaded from glTF, with simple collision shapes (boxes, cylinders, hulls).
- ~~Textured walls, floor and ceiling instead of the checker pattern.~~ Done: each level uses one random
  texture (chosen from the level seed) from the PNG files under `assets/textures/`, at most 128 × 128
  pixels, with nearest-neighbour filtering. Next: different textures for walls, floor and ceiling.
- Dear ImGui debug overlay.

## v0.3 — Party and rudimentary spellcasting

- **Basic party implementation**: 3 characters with randomly selected portraits. Each character has health,
  stamina, sanity. The party has shared mana.
- **Levitation spell**: turn off gravity for the party and allow vertical movement, as noclip does now
  but with collision.
- The party's light becomes a spell that can be toggled, with an intensity or radius.
- Audio: footsteps.

## Later (unordered)

- **More 3D level structure**: stairs (?), pits and bridges in generated levels.
- **Water**: water volumes in levels, transparent water surfaces, underwater fog and tint,
- **Swimming and diving** (including breath), buoyancy for floating objects.
- Full-screen mode, and mouse look.
- Monsters as billboard sprites, with Recast/Detour navigation.
- Attack spells, spell effects, projectiles that emit light (?).
- Items and inventory.
- Audio: ambient sound, spell sounds.
- Visibility culling (rooms and portals) if levels get large.
- **macOS support**: possible as long as the renderer runs on OpenGL 4.1 (Apple's newest, and
  deprecated, version). Needs testing on a Mac. If we move to OpenGL 4.5, macOS needs a separate 4.1
  path or another renderer.
- **Windows support**: add MSVC warning flags, and send logs to a file or console. SDL3, GLM, stb, glad
  and the C++ code are already portable, except reading console commands from the terminal, which uses POSIX
  `poll()` (`src/platform/terminal_input.cpp`).
- **Advanced party implementation**: The party is bound to a magical orb (?), and characters can transport
  themselves to a pocket dimension.
- New level layouts: areas with floating platforms and no walls, open areas, pseudo-platformer levels.
- Story and lore.

## Later (unordered, low priority, might not be done)

- **More static light sources**: braziers and glowing crystals, next to the torches.
- **Shadows of torches**: torch handles cast no shadows now. Under a point light just above the stick,
  a handle throws a hard, long shadow down the wall that a real torch, with its large flame, doesn't.
  A good-looking version needs a softer or larger light source.
- **Shadows of the player**: the player's light is at the camera, so its own shadows can't be seen, and
  the player has no body to cast shadows from other lights. Once there is a body (or a party), it could
  cast shadows from torches, which needs shadow maps that are drawn again whenever it moves.
