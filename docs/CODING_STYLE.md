# Coding style

Formatting and most rules are enforced by tools, so run them before committing:

```sh
cmake --build build --target format   # reformat all sources in place
cmake --build build --target lint     # check formatting, then run clang-tidy (fails on any issue)
scripts/lint_changed.py               # the same, but only for what changed since the last commit
ctest --test-dir build --output-on-failure
```

## Formatting and naming

- Formatting is defined in `.clang-format`: Google style, 4-space indent, 120 columns, braces on the same line.
- Includes are grouped into three blocks: standard library, other libraries, project headers.
- Naming is enforced by clang-tidy (`.clang-tidy`):

| What | Style | Example |
|---|---|---|
| Classes, structs, enums, type aliases | `PascalCase` | `LevelGenerator` |
| Functions, methods, variables, parameters, namespaces | `snake_case` | `build_world_mesh`, `light_count` |
| Private and protected members | `snake_case_` | `radius_` |
| Constants and enum values | `kPascalCase` | `kMaxPitch` |
| Macros | `UPPER_CASE` | `MANIFOLD_ASSERT` |

## Function complexity

clang-tidy limits the cognitive complexity of each function to 25 (`readability-function-cognitive-complexity`, with
its default threshold), and `lint` fails above it. Nested loops and conditions count the most. Keep functions below
the limit from the start by splitting them into functions with one job each, rather than splitting them only when
`lint` fails.

## Units and coordinates

- Lengths are in metres, angles in radians, time in seconds.
- z points up, and the lowest floor is at z = 0. Yaw is counter-clockwise from +x around +z, positive pitch looks up.
- Colors used in lighting are in linear RGB.
- Positions, velocities and transforms are 3D (`glm::vec3`), even where the game is currently 2D.

## Logging

- Write messages with `log_info`, `log_warning` and `log_error` from `src/util/log.hpp`, not with `std::cout`.
  They take `std::format` arguments and go to stderr with a `[info]`, `[warning]` or `[error]` prefix.
- Use a warning for something unusual that still works, such as a level that needed many generation attempts.

## Random numbers

- Make every random choice with the helpers of `src/util/random.hpp` on an `Rng` (`std::mt19937_64`):
  `random_index`, `random_int`, `random_float`, `random_unit`, `random_chance`, `random_weighted_index` and
  `random_shuffle`. Raw numbers of the engine (`rng()`) are fine where any 64-bit number will do, e.g. as an id that
  is mapped to a sound later.
- Don't use the standard distributions (`std::uniform_int_distribution`, `std::bernoulli_distribution`, ...),
  `std::shuffle`, `std::sample`, other engines or `std::rand`, also not in tests. The standard fixes the engines'
  output but not the distributions' algorithms, so the same seed would give other levels with another standard
  library, e.g. on Windows or macOS. The test `PortableRandomChoices` (`cmake/check_random.cmake`) fails if a source
  uses them. `std::random_device` is allowed only in `main.cpp`, for seeds that are meant to differ.
- If a new kind of choice is needed, add a helper to `src/util/random.hpp` with tests, rather than writing it inline.
  Its arithmetic must give the same bits everywhere: integers, or double operations that are rounded once. Write a
  multiplication followed by an addition as `std::fma`, since compilers fuse them into one instruction on some
  platforms only.
- The helpers make the choices portable, but math functions such as `std::cos` may still differ in their last bits
  between C libraries, so levels that depend on them may differ (see ROADMAP.md).
- Derive a separate generator for each independent choice from the seed with its own salt (`Rng rng(seed ^ kSalt)`),
  so that adding random numbers to one part doesn't change the others.
- Changing a helper, or the order of the choices of level generation, changes the levels of all seeds: increase the
  save format version (`kSaveFormatVersion`), and update the pinned results in `tests/random_test.cpp` only on
  purpose.

## Documentation comments

Doc comments use Doxygen syntax in the `///` style. They are shown by editors through clangd, and
clang-tidy checks them with `-Wdocumentation`, so for example a `\param` that doesn't match a parameter
name fails `lint`.

```cpp
/// Turns the player's view.
///
/// Yaw is kept within [0, 2 pi), pitch is clamped to [-kMaxPitch, kMaxPitch].
///
/// \param delta_yaw   Change of yaw in radians. Positive turns left.
/// \param delta_pitch Change of pitch in radians. Positive looks up.
void rotate(float delta_yaw, float delta_pitch);
```

### What to document

- **Every public class, struct and function in a header** gets at least a one-sentence summary.
  - The first sentence is the summary. Further details go into a new paragraph after an empty `///` line.
  - Self-explanatory accessors can stay without a comment (`world()`, `set_position()`), or get one line
    if there is anything to add, such as units or valid range.
- **Struct fields** get a trailing `///<` comment, or a `///` comment above if it doesn't fit on the line,
  when their meaning, units or range isn't obvious from the name.
- **`\param`** only when it adds something: units, sign conventions, valid ranges, ownership. Skip it for
  obvious parameters, but once one parameter of a function is documented, document all of them.
  Use `\param[out]` for output parameters.
- **`\return`** when the return value needs explaining, e.g. what an empty `std::optional` or `false`
  means.
- **`\throws`** whenever a function can throw, with the exception type and when it is thrown.
- **Helpers in `.cpp` files** (in an anonymous namespace) get a short `///` summary as well.
- Declarations are documented in the header, not repeated at the definition.

### Other comments

- Comments inside function bodies use `//` and explain *why* something is done, or summarize a block of
  steps. Don't restate what the code obviously does.
- File-level notes that don't belong to a declaration, such as conventions for a whole header, use `//`.

### Language

- Write in plain, simple English with short sentences.
- Prefer concrete words: "in metres", "positive turns left", "returns false if ...".
- State units, coordinate conventions and ranges explicitly.
- Refer to code in backticks: `keep_clear`, `GameConfig::move_speed`.

## Python scripts

Helper scripts in `scripts/` are written in Python 3.11+. Most need only the standard library; the third-party
packages that some need, such as Pillow, and the development tools (black, ruff and pyrefly) are set up as a Poetry
project in `scripts/pyproject.toml`. A script that imports a third-party package is run with
`poetry run python NAME.py` from `scripts/` (`poetry -C scripts run` would also run it there, so relative paths in its
arguments would be wrong), and its docstring says so.

```sh
cd scripts
poetry install            # once, to install the packages and tools
poetry run black .        # reformat the scripts in place
poetry run ruff check .   # lint, including docstrings
poetry run pyrefly check  # check types
```

- Formatting is black's with 120 columns. Imports are sorted as ruff's isort rules want.
- Functions have type annotations on parameters and return values.
- Every module and function has a docstring in reStructuredText. The first line is the summary, further details
  go into new paragraphs. Parameters, return values and exceptions are documented with `:param name:`,
  `:returns:` and `:raises Type:` fields, following the same rules as for C++ above: `:param:` only when it adds
  something, and `:raises:` for the failures a caller can expect, including `SystemExit` from `sys.exit` and
  argparse. Failures of the environment, such as a missing `ffmpeg` or `git`, need not be listed.
- The module docstring describes the script and how to run it. It, or its first paragraph, is the description
  shown by `--help`, so keep it readable as plain text: little markup beyond `Usage::` with an indented example
  for literal blocks.
