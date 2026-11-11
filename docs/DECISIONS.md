# Manifold — Decisions

Important technical decisions, newest last. Each entry says what was decided, why, and what it costs.
Plans that follow from a decision are in [ROADMAP.md](ROADMAP.md).

## 2026-11-11: Use OpenGL 4.5 core instead of 3.3

Decision: move the renderer from OpenGL 3.3 core to 4.5 core at the start of v0.2.

Gains:

- **Debug output** (`glDebugMessageCallback`): GL errors and misuse are reported at once, with a message,
  through our logger.
- **Direct State Access**: buffers, textures and framebuffers can be changed without binding them first.
  The renderer code is shorter and has fewer bugs from hidden GL state.
- **Cubemap arrays**: shadow maps for several point lights fit in one texture, which suits the planned
  shadows.
- **Shader storage buffers and compute shaders**: a long light list and clustered forward lighting become
  possible, if we ever need many lights.

Why 4.5 and not 4.6: 4.6 adds little we would use (SPIR-V shaders, anisotropic filtering in core), while
4.5 also runs on Mesa's software renderer (llvmpipe), which VMs and headless CI use.

Drawbacks:

- **macOS can't be supported in the near future.** Apple stopped at OpenGL 4.1. Supporting it would need a
  Vulkan (MoltenVK) or Metal renderer.
- Very old GPUs, virtual machine GPUs (virgl) and ARM boards such as the Raspberry Pi can't run the game.
- The setup code in the renderer must be rewritten for DSA, and the shaders moved to `#version 450`.

## 2026-11-11: Use Jolt Physics for collision

Decision: replace our own collision code with Jolt Physics at the start of v0.2. The player becomes a
Jolt `CharacterVirtual`.

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
- Alternative that was considered: keep our own code and add a spatial grid for speed. This is cheaper
  now, but stairs, props, doors and water would each need more of our own physics later.
