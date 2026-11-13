# Instructions for coding agents

Manifold is a hobby first-person fantasy game in C++20, built with CMake. See [README.md](README.md) for
building and running, [docs/ROADMAP.md](docs/ROADMAP.md) for plans and technical decisions,
[docs/DECISIONS.md](docs/DECISIONS.md) for important decisions with their reasons, and
[GAME_MANUAL.md](GAME_MANUAL.md) for controls.

## Conventions

Follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md): formatting, naming, units and coordinates, and how
code is documented with `///` comments.

## Before finishing a change

```sh
cmake --build build --target format
cmake --build build                       # must build without warnings
ctest --test-dir build --output-on-failure
scripts/lint_changed.py                   # must pass
```

`scripts/lint_changed.py` runs the checks of the `lint` target only on the sources changed since the last
commit, and on the .cpp files that include a changed header. It lints everything if `.clang-tidy`,
`.clang-format` or `CMakeLists.txt` changed. After committing, use `--base HEAD~1` (or another commit), or
name the files to lint. The full `cmake --build build --target lint` takes a few minutes; use it only when
asked or when unsure what a change affects.

- Add or update unit tests in `tests/` for new logic. Code that needs a window or OpenGL is kept out of
  `manifold_core`, so that everything else can be tested.
- When changing key bindings or cheats, update [GAME_MANUAL.md](GAME_MANUAL.md).
- If a new system package is needed, add it to the install command in [README.md](README.md).
- `./build/manifold SEED --screenshot FILE.png` saves the first frame, which helps to check rendering changes.

## Review after nontrivial changes

After a nontrivial code change (not typos, comments or docs only), and after the checks above pass, commit
the change and then start one subagent to review it:

- Give the subagent the commit (or diff) to review and the goal of the change.
- Tell it not to use any review skill (such as `/code-review`) and not to start subagents of its own.
- Tell it that formatting, build, tests and lint already passed, so it shouldn't run them again. It may
  build or run other small checks if a specific finding needs them.
- Ask it to report concrete findings (bugs, missed cases, docs that no longer match the code, departures
  from the conventions), each with a file and line, most severe first.

Then address the findings:

- Fix the clear ones, normally in a separate commit after the reviewed one, and run the checks again.
- If a finding needs a decision that isn't obvious (a design trade-off, a change of scope, a disagreement
  with the reviewer), ask the user before acting on it.
- Tell the user which findings were fixed, which were rejected and why.
