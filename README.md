# Manifold

A hobby first-person fantasy game about playing as a mage. See [docs/roadmap.md](docs/roadmap.md).

## Building (Linux)

Requirements: a C++20 compiler (GCC 13+ or Clang 16+), CMake 3.24+, and optionally Ninja.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/manifold
```
