# Manifold

[![CI](https://github.com/igor-buzhinsky/manifold/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/igor-buzhinsky/manifold/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/igor-buzhinsky/manifold)](LICENSE)

A hobby first-person fantasy game about playing as a mage. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Building (Linux)

Requirements: a C++20 compiler (GCC 13+ or Clang 16+), CMake 3.25+, optionally Ninja, and a GPU with OpenGL 4.1
or newer (any GPU from about 2012 on).
On Ubuntu 24.04, everything needed to build and lint the project is installed with:

```sh
sudo apt install build-essential cmake ninja-build git clang-format clang-tidy pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl1-mesa-dev libgl1-mesa-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev pipx
```

The first line is the toolchain (pkg-config is used by SDL3's build). The rest are system development packages
that SDL3 needs to talk to X11/Wayland, the GPU, audio, the desktop (D-Bus) and input devices (udev).
The full list of SDL's optional dependencies is in its
[README-linux](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies). SDL3 itself and the other libraries
(GLM, stb including stb_vorbis, Dear ImGui, GoogleTest) are downloaded and built by CMake. The OpenGL loader (glad) is generated code in
`external/glad/`.

Build and run:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/manifold                            # random level
./build/manifold 42                         # level generated from seed 42
./build/manifold 42 --layout rooms          # force the layout of the first level: debug or rooms (by default rooms; debug only on request)
./build/manifold 42 --screenshot view.png   # save the first frame as a PNG and exit
./build/manifold 42 --exec "lighting off; screenshot flat.png 800 450; quit"   # run console commands
```

The game reads its textures from `assets/textures/` and its sounds from `assets/sounds/` in the source tree,
whose path is built into the executable, so keep the checkout where it was built. If no texture is found,
surfaces are plain grey. Without an audio output device, the game runs without sound.

Controls and console commands are described in [GAME_MANUAL.md](GAME_MANUAL.md). Press Escape or close the
window to quit.

## Tests

Unit tests use GoogleTest, which CMake downloads. Build as above, then:

```sh
ctest --test-dir build --output-on-failure
```

Pass `-DMANIFOLD_BUILD_TESTS=OFF` to the first `cmake` command to skip building the tests, or
`-DMANIFOLD_WARNINGS_AS_ERRORS=ON` to fail on compiler warnings in Manifold's own code.

[CI](.github/workflows/ci.yml) builds with warnings as errors, runs the tests, and runs the `lint` target and the
Python checks on each push to `main` and on each pull request.

## Code style

Formatting is defined in `.clang-format` (Google style, 4-space indent, 120 columns) and
lint rules, including naming conventions, in `.clang-tidy`.

```sh
cmake --build build --target format   # reformat all sources in place
cmake --build build --target lint     # check formatting, then run clang-tidy (fails on any issue)
scripts/lint_changed.py               # the same, but only for what changed since the last commit
```

The Python scripts in `scripts/` are formatted with black and checked with ruff and pyrefly, installed with
[Poetry](https://python-poetry.org/) 2.0 or newer (`pipx install poetry`): run `poetry install` once in `scripts/`, then
`poetry run black .`, `poetry run ruff check .` and `poetry run pyrefly check` there.

See [docs/CODING_STYLE.md](docs/CODING_STYLE.md) for all conventions, including doc comments.

Naming: types `PascalCase`; functions, variables and namespaces `snake_case`; private and
protected members `snake_case_`; constants and enum values `kPascalCase`; macros `UPPER_CASE`.

## Credits

Third-party assets in `assets/`. Each pack's directory has a `README.md` with its source, license, the files
taken and how they were processed. A pack whose files are in several groups, such as `assets/textures/stone/` and
`assets/textures/wood/`, has a directory in each.

- Textures: [Tiny Texture Pack 1](https://screamingbrainstudios.com/sbs_-_tiny_texture_pack_1_-_128x128/) and
  [Tiny Texture Pack 2](https://screamingbrainstudios.com/sbs_-_tiny_texture_pack_2_-_128x128/) by
  Screaming Brain Studios, CC0 1.0.
- Sounds: [Digital Audio](https://kenney.nl/assets/digital-audio) and
  [RPG Audio](https://kenney.nl/assets/rpg-audio) by Kenney (kenney.nl), CC0 1.0.
- Footstep sounds: from [Freesound](https://freesound.org/), CC0 1.0. The author of each is in the README next to
  it, under `assets/sounds/footsteps/`.

## License

The source code of Manifold is licensed under the [Mozilla Public License 2.0](LICENSE). Third-party files keep
their own licenses: the assets in `assets/` (see Credits above), the generated OpenGL loader in `external/glad/`,
and the libraries that CMake downloads (glm, SDL3, stb, Dear ImGui, nlohmann/json and googletest).
