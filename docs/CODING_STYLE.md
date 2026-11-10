# Coding style

Formatting and most rules are enforced by tools, so run them before committing:

```sh
cmake --build build --target format   # reformat all sources in place
cmake --build build --target lint     # check formatting, then run clang-tidy (fails on any issue)
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

## Units and coordinates

- Lengths are in metres, angles in radians, time in seconds.
- z points up, and the lowest floor is at z = 0. Yaw is counter-clockwise from +x around +z, positive pitch looks up.
- Colors used in lighting are in linear RGB.
- Positions, velocities and transforms are 3D (`glm::vec3`), even where the game is currently 2D.

## Logging

- Write messages with `log_info`, `log_warning` and `log_error` from `src/util/log.hpp`, not with `std::cout`.
  They take `std::format` arguments and go to stderr with a `[info]`, `[warning]` or `[error]` prefix.
- Use a warning for something unusual that still works, such as a level that needed many generation attempts.

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
