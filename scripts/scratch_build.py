#!/usr/bin/env python3
"""Builds a throwaway C++ program against manifold_core, e.g. to try an idea or to check old code.

Compiles FILE.cpp, which can be anywhere (e.g. in a scratch directory outside the repository), with the
flags that `manifold_sim` is compiled with, and links it with the libraries `manifold_sim` is linked with:
manifold_core and its dependencies. The program can include any header under src/, such as "game.hpp".
Warnings are shown but are not errors.

First builds the `manifold_sim` target, so that manifold_core matches the sources in the working tree.
To check that a test or a program fails on old code, check out or stash the old sources, run this script
again, and compare.

Needs a build directory configured with the Ninja generator, as in README.md.

Usage::

    scripts/scratch_build.py /tmp/try.cpp                  # builds /tmp/try next to it
    scripts/scratch_build.py /tmp/try.cpp -o /tmp/old      # builds /tmp/old
    scripts/scratch_build.py /tmp/try.cpp --run 10 20      # builds it, then runs it with arguments 10 and 20
"""

import argparse
import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# The target whose compile flags and libraries a scratch program gets: it links manifold_core and nothing else.
TARGET = "manifold_sim"
TARGET_SOURCE = ROOT / "src" / "sim" / "sim_main.cpp"


def run_checked(command: list[str], cwd: Path) -> str:
    """Runs a command and returns its output.

    :param command: The program and its arguments.
    :param cwd: The directory to run it in.
    :returns: What it printed to stdout.
    :raises SystemExit: If it fails. Its error messages go to stderr.
    """
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, text=True)  # Errors go to stderr.
    if result.returncode:
        sys.exit(f"{shlex.join(command)} failed")
    return result.stdout


def compile_command(build_dir: Path, source: Path, obj: Path) -> list[str]:
    """The command that compiles ``source`` as TARGET's sources are compiled, without warnings as errors.

    :param build_dir: The CMake build directory.
    :param source: The file to compile.
    :param obj: The object file to write.
    :returns: The command, to run in ``build_dir``.
    :raises SystemExit: If the compilation database has no entry for TARGET_SOURCE.
    """
    database = json.loads((build_dir / "compile_commands.json").read_text())
    entries = [entry for entry in database if Path(entry["file"]) == TARGET_SOURCE]
    if not entries:
        sys.exit(f"{TARGET_SOURCE} is not in {build_dir / 'compile_commands.json'}; configure with CMake first")
    words = shlex.split(entries[0]["command"])
    command: list[str] = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ("-o", "-c"):
            skip = True
        elif word != "-Werror":
            command.append(word)
    return [*command, "-o", str(obj), "-c", str(source)]


def link_command(build_dir: Path, obj: Path, output: Path) -> list[str]:
    """The command that links ``obj`` into ``output`` with TARGET's libraries.

    :param build_dir: The CMake build directory, configured with Ninja.
    :param obj: The object file of the program.
    :param output: The executable to write.
    :returns: The command, to run in ``build_dir``.
    :raises SystemExit: If Ninja can't give TARGET's link command.
    """
    lines = run_checked(["ninja", "-t", "commands", TARGET], build_dir).splitlines()
    if not lines:
        sys.exit(f"ninja gives no commands for {TARGET}")
    # The last command links TARGET, e.g. ": && c++ -g main.o -o manifold_sim libmanifold_core.a ... && :".
    words = [word for word in shlex.split(lines[-1]) if word not in (":", "&&")]
    command: list[str] = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word == "-o":
            skip = True
        elif word.endswith(".o"):
            command.append(str(obj))
        else:
            command.append(word)
    return [*command, "-o", str(output)]


def main() -> None:
    """Builds the program, and runs it if asked.

    :raises SystemExit: If building or running fails, with the program's exit code if it ran.
    """
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("source", type=Path, help="the .cpp file to build")
    parser.add_argument("-o", "--output", type=Path, help="the executable (default: the source without .cpp)")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build", help="the CMake build directory")
    parser.add_argument(
        "--run",
        nargs=argparse.REMAINDER,
        metavar="ARG",
        help="run the program after building it, with the arguments after --run",
    )
    options = parser.parse_args()

    source = options.source.resolve()
    if not source.is_file():
        sys.exit(f"{source} is not a file")
    output = (options.output or source.with_suffix("")).resolve()
    obj = output.with_suffix(".o")
    build_dir = options.build_dir.resolve()

    run_checked(["cmake", "--build", str(build_dir), "--target", TARGET], ROOT)
    run_checked(compile_command(build_dir, source, obj), build_dir)
    run_checked(link_command(build_dir, obj, output), build_dir)
    obj.unlink()
    print(f"Built {output}", file=sys.stderr)
    if options.run is not None:
        sys.exit(subprocess.run([str(output), *options.run]).returncode)


if __name__ == "__main__":
    main()
