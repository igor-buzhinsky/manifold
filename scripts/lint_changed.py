#!/usr/bin/env python3
"""Lints only the sources that changed: the same checks as the `lint` target, but faster.

Checks the formatting of the changed sources with clang-format, then runs clang-tidy on the changed .cpp
files and on every .cpp file that includes a changed header, directly or through other headers.

By default, "changed" means different from HEAD: staged, unstaged and untracked files under src/ and
tests/. Use --base to compare with another commit, or name the files to lint. If .clang-tidy,
.clang-format or CMakeLists.txt changed, all sources are linted, since any of them may be affected.

Uses the tools found by CMake (see CMakeCache.txt in the build directory) and its compilation database,
so only files that are built are linted: tests only when MANIFOLD_BUILD_TESTS is on.

Usage:
    scripts/lint_changed.py                     # what changed since the last commit
    scripts/lint_changed.py --base HEAD~1       # also what the last commit changed
    scripts/lint_changed.py src/game.hpp        # these files, and the .cpp files that include them
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRS = ("src", "tests")
SOURCE_SUFFIXES = (".cpp", ".hpp")
# Changes to these files may affect the lint result of any source.
CONFIG_FILES = (".clang-tidy", ".clang-format", "CMakeLists.txt")
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def git_lines(*args: str) -> list[str]:
    """Runs git in the repository and returns the lines of its output."""
    output = subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout
    return [line for line in output.splitlines() if line]


def changed_files(base: str) -> list[str]:
    """Paths, relative to the repository, that differ from `base` or are untracked."""
    return sorted(set(git_lines("diff", "--name-only", base)) | set(git_lines("ls-files", "--others",
                                                                                "--exclude-standard")))


def all_sources() -> list[Path]:
    """All .cpp and .hpp files under the source directories."""
    return sorted(path for folder in SOURCE_DIRS for path in (ROOT / folder).rglob("*")
                  if path.suffix in SOURCE_SUFFIXES and path.is_file())


def resolve_include(name: str, including: Path) -> Path | None:
    """The source file that `#include "name"` in `including` refers to, if it is one of ours."""
    for folder in (including.parent, *(ROOT / folder for folder in SOURCE_DIRS)):
        candidate = (folder / name).resolve()
        if candidate.is_file():
            return candidate
    return None


def translation_units(changed: set[Path], sources: list[Path]) -> set[Path]:
    """The .cpp files among `changed`, plus those that include a changed file, directly or not."""
    included_by: dict[Path, set[Path]] = {}
    for source in sources:
        for name in INCLUDE.findall(source.read_text(encoding="utf-8")):
            header = resolve_include(name, source)
            if header is not None:
                included_by.setdefault(header, set()).add(source)
    affected = set(changed)
    pending = list(changed)
    while pending:
        for includer in included_by.get(pending.pop(), set()):
            if includer not in affected:
                affected.add(includer)
                pending.append(includer)
    return {path for path in affected if path.suffix == ".cpp"}


def cmake_cache(build_dir: Path) -> dict[str, str]:
    """The entries of the CMake cache, by name without type."""
    entries = {}
    for line in (build_dir / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Za-z0-9_]+):[A-Z]+=(.*)$", line)
        if match:
            entries[match.group(1)] = match.group(2)
    return entries


def tool(cache: dict[str, str], name: str, package: str) -> str:
    """The path of a tool found by CMake, or exits with a hint on how to install it."""
    path = cache.get(name, "")
    if not path or path.endswith("-NOTFOUND"):
        sys.exit(f"{name} not found: sudo apt install {package}, then re-run cmake -S . -B build")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("files", nargs="*", help="files to lint instead of the changed ones")
    parser.add_argument("--base", default="HEAD", help="commit to compare with (default: HEAD)")
    parser.add_argument("--build-dir", default=str(ROOT / "build"), help="CMake build directory (default: build)")
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()
    cache = cmake_cache(build_dir)
    sources = all_sources()

    names = [str(Path(name).resolve().relative_to(ROOT)) for name in args.files] or changed_files(args.base)
    if any(name in CONFIG_FILES for name in names):
        print("Lint configuration or build changed: linting all sources", flush=True)
        changed = set(sources)
    else:
        changed = {(ROOT / name).resolve() for name in names} & set(sources)
    if not changed:
        print("No changed sources to lint", flush=True)
        return 0

    to_format = sorted(str(path) for path in changed)
    print(f"Checking formatting of {len(to_format)} file(s)", flush=True)
    if subprocess.run([tool(cache, "CLANG_FORMAT_EXE", "clang-format"), "--dry-run", "--Werror", *to_format]).returncode:
        return 1

    entries = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    database = {(Path(entry["directory"]) / entry["file"]).resolve() for entry in entries}
    units = translation_units(changed, sources)
    for skipped in sorted(units - database):
        print(f"Not in the compilation database, not linted: {skipped.relative_to(ROOT)}", flush=True)
    units = sorted(units & database)
    if not units:
        print("No translation units to lint", flush=True)
        return 0
    listed = " ".join(str(unit.relative_to(ROOT)) for unit in units)
    print(f"Running clang-tidy on {len(units)} file(s): {listed}", flush=True)
    # run-clang-tidy takes regexes on paths in the compilation database; anchoring them keeps out other files.
    regexes = ["^" + re.escape(str(unit)) + "$" for unit in units]
    return subprocess.run([tool(cache, "RUN_CLANG_TIDY_EXE", "clang-tidy"), "-quiet", "-clang-tidy-binary",
                           tool(cache, "CLANG_TIDY_EXE", "clang-tidy"), "-p", str(build_dir), *regexes]).returncode


if __name__ == "__main__":
    sys.exit(main())
