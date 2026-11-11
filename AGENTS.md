# Instructions for coding agents

Manifold is a hobby first-person fantasy game in C++20, built with CMake. See [README.md](README.md) for
building and running, [ROADMAP.md](ROADMAP.md) for plans and technical decisions, and
[GAME_MANUAL.md](GAME_MANUAL.md) for controls.

## Conventions

Follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md): formatting, naming, units and coordinates, and how
code is documented with `///` comments.

## Before finishing a change

```sh
cmake --build build --target format
cmake --build build                       # must build without warnings
ctest --test-dir build --output-on-failure
cmake --build build --target lint         # must pass
```

- Add or update unit tests in `tests/` for new logic. Code that needs a window or OpenGL is kept out of
  `manifold_core`, so that everything else can be tested.
- When changing key bindings or cheats, update [GAME_MANUAL.md](GAME_MANUAL.md).
- If a new system package is needed, add it to the install command in [README.md](README.md).
- `./build/manifold SEED --screenshot FILE.png` saves the first frame, which helps to check rendering changes.
