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
  SDL3 uses it to find Wayland, PipeWire and PulseAudio.) The same code
  builds on Linux, Windows and macOS, which helps both of those "later" items.
- **The version is checked at compile time**: the header only declares functions of OpenGL 4.1 core, so
  using a newer function by mistake (for example one from 4.5) doesn't compile. With libepoxy, such code
  compiles and fails only on a GPU or OS without that function, such as macOS.
- Small and simple: one header and one source file, with no library to link.

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
