#!/usr/bin/env python3
"""Cuts parts of a sound file into separate OGG Vorbis files, e.g. single footsteps from a recording of several.

Each part gets a short fade in and out, so that it starts and ends without a click, and is scaled so that its
loudest sample is at --peak dBFS, so that all parts sound about equally loud. Needs ffmpeg.

Usage::

    scripts/cut_sounds.py ORIGINAL OUTPUT_PREFIX START-END [START-END ...]

Writes OUTPUT_PREFIX-01.ogg, OUTPUT_PREFIX-02.ogg, ... one for each START-END, in seconds from the start of
ORIGINAL. For example::

    scripts/cut_sounds.py steps.wav assets/sounds/footsteps/stone/freesound-org/813622-step 0.01-0.27 1.02-1.53
"""

import argparse
import array
import math
import subprocess
import sys

FADE_IN = 0.005  # Seconds.
FADE_OUT = 0.03  # Seconds.


def trim_filter(start: float, end: float) -> str:
    """An ffmpeg filter that keeps a part of the input, with times starting at 0 again.

    The whole file is decoded and trimmed in the filter, instead of seeking with ``-ss`` and ``-to``: seeking in an
    MP3 skips the decoder's start-up, so the first tens of milliseconds of a part came out silent.

    :param start: Start of the part, in seconds from the start of the input.
    :param end: End of the part, in seconds from the start of the input.
    :returns: The filter, for ffmpeg's ``-af`` option.
    """
    return f"atrim=start={start}:end={end},asetpts=PTS-STARTPTS"


def peak_dbfs(original: str, start: float, end: float) -> float:
    """The level of the loudest sample of a part of a sound file, in dBFS.

    :param original: Path of the sound file, in any format ffmpeg reads.
    :param start: Start of the part, in seconds from the start of the file.
    :param end: End of the part, in seconds from the start of the file.
    :returns: The level, or ``-math.inf`` if the part is silent.
    :raises subprocess.CalledProcessError: If ffmpeg fails, e.g. because it can't read the file.
    """
    pcm = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", original, "-af", trim_filter(start, end), "-f", "s16le", "-"],
        capture_output=True,
        check=True,
    ).stdout
    samples = array.array("h")
    samples.frombytes(pcm[: len(pcm) // 2 * 2])
    peak = max((abs(s) for s in samples), default=0)
    return 20 * math.log10(peak / 32768) if peak > 0 else -math.inf


def main() -> int:
    """Cuts the parts given on the command line and writes them as OGG files.

    :returns: The exit code: 0 on success, 1 if a part is too short or silent.
    :raises SystemExit: With code 2 if the arguments are wrong.
    :raises subprocess.CalledProcessError: If ffmpeg fails.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("original", help="the sound file to cut, in any format ffmpeg reads")
    parser.add_argument("prefix", help="path of the outputs without the number and .ogg")
    parser.add_argument("parts", nargs="+", metavar="START-END", help="a part to cut, in seconds")
    parser.add_argument("--peak", type=float, default=-6.0, help="level of the loudest sample of each part, dBFS")
    parser.add_argument("--quality", type=float, default=5.0, help="Vorbis quality, from -1 to 10")
    args = parser.parse_args()

    for number, part in enumerate(args.parts, start=1):
        try:
            start, end = (float(x) for x in part.split("-"))
        except ValueError:
            parser.error(f"a part must be START-END in seconds, e.g. 1.5-2.25, not {part}")
        if end <= start + FADE_IN + FADE_OUT:
            print(f"part {part} is too short", file=sys.stderr)
            return 1
        gain = args.peak - peak_dbfs(args.original, start, end)
        if not math.isfinite(gain):
            print(f"part {part} is silent", file=sys.stderr)
            return 1
        length = end - start
        filters = (
            f"{trim_filter(start, end)},afade=t=in:d={FADE_IN},"
            f"afade=t=out:st={length - FADE_OUT:.3f}:d={FADE_OUT},volume={gain:.2f}dB"
        )
        output = f"{args.prefix}-{number:02d}.ogg"
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-i",
                args.original,
                "-af",
                filters,
                "-map_metadata",
                "-1",
                "-c:a",
                "libvorbis",
                "-q:a",
                str(args.quality),
                output,
            ],
            check=True,
        )
        print(f"{output}: {start:.2f}-{end:.2f} s, {gain:+.1f} dB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
