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
- **CMake**, pulling dependencies with FetchContent (or vcpkg if that becomes painful).
- Small libraries are vendored where practical, to keep the build simple.

### Libraries

| Concern | Choice | Notes |
|---|---|---|
| Window, input, full-screen, audio | **SDL3** | Works well on Wayland and X11. |
| Rendering API | **OpenGL 4.x core** (3.3 as fallback) | Vulkan is not worth the effort for this scope. |
| GL loader | **glad** | |
| Math | **GLM** | |
| Textures | **stb_image** | |
| Models | **cgltf** (glTF 2.0) | glTF is the single asset format. Blender converts other formats to it. |
| Physics and collision | **Jolt Physics** | Used from v0.1, see below. |
| Debug UI | **Dear ImGui** | Live tuning of lights, generator parameters, and so on. |
| Navigation (later) | **Recast/Detour** | Navmeshes for monsters, including multi-level layouts. |
| Audio (later) | SDL3 audio or **miniaudio** | |

Rejected alternatives:

- **raylib**: the fastest start, but HDR and multi-light shadows would mean working around it.
- **Godot**: gives most features for free, but most code would be GDScript instead of C++.

### Architecture principles: keep 3D paths open

Early versions restrict movement to a plane. Even so, the core must stay fully 3D, so that
z-axis movement, water, levitation and arbitrary 3D objects can be added later without rewrites.

- All positions, velocities and transforms are `vec3`. Nothing in the game logic assumes `(x, y)`.
- No grid or raycaster renderer. The renderer draws arbitrary triangle meshes.
- **Jolt is used from v0.1**, instead of custom 2D collision code. The player is a
  capsule driven by Jolt's `CharacterVirtual`. Early versions just keep the player on the floor.
  Later, Jolt also provides cylinders, convex hulls, buoyancy and raycasts.
- A level is a 3D data structure: render meshes, collision shapes, lights, and volumes
  (for example water) in the future. The generator may work in 2D internally and extrude the
  result, but nothing downstream depends on that.
- Level generation is deterministic from a seed, so levels can be reproduced for debugging.

### Rendering and lighting principles

- **Linear-space lighting** from the start: sRGB textures and an sRGB framebuffer.
- **HDR render target with tonemapping.** Then a level with zero ambient light is truly dark.
- Point lights with inverse-square falloff and a smooth range cutoff.
- Omnidirectional shadows from cubemap shadow maps, limited to the nearest N lights (about 4–8).
- Forward rendering first. Move to clustered forward or deferred only if the light count requires it.
- Retro look: low-res textures with nearest-neighbour filtering, optional low-res render target,
  distance fog, billboard sprites for items and creatures.

## v0.1 — Exploration prototype

Scope: walk around a randomly generated level in the dark with a light at the player.

- SDL3 window with windowed and full-screen modes. OpenGL context, ImGui debug overlay.
- First-person camera: mouse look and WASD movement on a plane (no jumping, no z-axis movement).
- **Level generator**: random polygonal obstacles inside a bounded area, extruded to walls,
  with a floor and a ceiling. Seed-based.
  - Check that the whole free area is connected and the player start is not inside an obstacle.
- Textured walls, floor and ceiling.
- Collision through Jolt: a capsule character against static level geometry.
- **Lighting**: one point light at the player's position plus a configurable ambient term
  (can be set to zero). Linear space, sRGB, HDR, and tonemapping are set up from the start.
  - Shadows are not needed in v0.1. A light at the camera position lights exactly the
    surfaces the camera can see, so its shadows are never visible. This changes once the
    light is offset (for example held in a hand) or other lights are added.
- A level exit that regenerates the level with a new seed. This is a minimal version of the
  linear series of levels.

## v0.2 — Lighting and level structure

- **Multiple point lights**: static light sources placed in the level (torches, braziers,
  glowing crystals), with color and flicker.
- **Shadows**: cubemap shadow maps for the nearest N lights.
- The player's light becomes a spell that can be toggled, with an intensity or radius.
- **Generator logic**: rooms and corridors (for example BSP or graph-based layouts) instead of
  scattered obstacles. A guaranteed path from start to exit, and placement of light sources
  and decorative objects according to rules.
- First static 3D props loaded from glTF, with simple collision shapes (boxes, cylinders, hulls).

## Later (unordered)

- **3D movement**: jumping, multiple floor heights, stairs, ramps, pits and bridges in
  generated levels.
- **Levitation spell**: turn off gravity for the character controller and allow vertical movement.
- **Water**: water volumes in levels, transparent water surfaces, underwater fog and tint,
  **swimming and diving** (including breath), buoyancy for floating objects.
- Monsters as billboard sprites, with Recast/Detour navigation.
- Attack spells, spell effects, projectiles that emit light.
- Items and inventory.
- Audio: ambient sound, footsteps, spell sounds.
- Visibility culling (rooms and portals) if levels get large.
- Party support: 3 characters that are bound to a magical orb and can transport themselves to a pocket dimension.
- Character stats: health, stamina, sanity, shared mana.
- Story and lore.