# Manifold

A hobby first-person fantasy game about playing as a mage. See [docs/roadmap.md](docs/roadmap.md).

## Building (Linux)

Requirements: a C++20 compiler (GCC 13+ or Clang 16+), CMake 3.24+, and optionally Ninja.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/manifold
```

## Code style

Formatting is defined in `.clang-format` (Google style, 4-space indent, 100 columns) and
lint rules, including naming conventions, in `.clang-tidy`. Both tools are needed:

```sh
sudo apt install clang-format clang-tidy
```

```sh
cmake --build build --target format   # reformat all sources in place
cmake --build build --target lint     # check formatting, then run clang-tidy (fails on any issue)
```

Naming: types `PascalCase`; functions, variables and namespaces `snake_case`; private and
protected members `snake_case_`; constants and enum values `kPascalCase`; macros `UPPER_CASE`.
