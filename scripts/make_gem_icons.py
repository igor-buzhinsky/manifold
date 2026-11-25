#!/usr/bin/env python3
r"""Makes the icons of gems from the sheet "Six grades of gems" by Hansjörg Malthaner, which shows six red gems.

The sheet is ``hjm-6_grades_of_gems_alpha.png`` from https://opengameart.org/content/six-grades-of-gems, 210 x 140
pixels with a transparent background, the gems in three columns and two rows. Each gem of the game takes another of
them, except the cut-off one at the bottom right, and recolors it: the ruby keeps its color, the emerald, sapphire
and amethyst get their hue turned, and the diamond loses most of its saturation and is brightened. Each is cut out and
scaled with Lanczos interpolation into the middle of two transparent squares of the same size, the icons of a large and
a small gem. A small gem is worth a quarter of a large one, so it has a quarter of its volume: it is drawn smaller by
the cube root of 4, about 1.59. The large gem fills LARGE_FILL of its icon, which leaves a margin around it. An icon
that is scaled up is sharpened a little. The icons are written as ``GEM-large.png`` and ``GEM-small.png`` in RGBA
without metadata.

Needs Pillow, so run it with ``poetry run`` from this folder, not with ``poetry -C scripts run`` from elsewhere: that
also runs it in this folder, so relative paths in the arguments would be wrong.

Usage::

    cd scripts
    poetry run python make_gem_icons.py ORIGINAL OUTPUT_DIR [--size SIZE]

For example::

    poetry run python make_gem_icons.py ~/Downloads/hjm-6_grades_of_gems_alpha.png \
        ../assets/inventory/gems/opengameart-org-six-grades-of-gems
"""

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageFilter

LARGE_FILL = 0.9  # Width or height of a large gem, whichever is larger, as a fraction of its icon's.
SMALL_FILL = LARGE_FILL / 4 ** (1 / 3)  # The same of a small gem, which has a quarter of the volume of a large one.


@dataclass(frozen=True)
class Gem:
    """How one gem of the game is made from the sheet."""

    name: str  # Name of the gem, which starts the names of its files.
    column: int  # Column of the gem on the sheet, from 0 at the left.
    row: int  # Row of the gem on the sheet, from 0 at the top.
    hue_shift: int  # How far to turn the hue, in Pillow's units of 1/256 of a full turn.
    saturation: float  # Factor of the saturation.
    value_gamma: float  # Exponent of the brightness from 0 to 1: below 1 brightens, above 1 darkens.


# The red of the sheet has a hue of about 5 of 256 (7 degrees).
GEMS = [
    Gem("ruby", column=0, row=0, hue_shift=0, saturation=1.0, value_gamma=1.0),
    Gem("emerald", column=1, row=0, hue_shift=95, saturation=0.95, value_gamma=0.95),
    Gem("sapphire", column=2, row=0, hue_shift=158, saturation=0.95, value_gamma=0.85),
    Gem("amethyst", column=0, row=1, hue_shift=188, saturation=0.85, value_gamma=0.95),
    Gem("diamond", column=1, row=1, hue_shift=145, saturation=0.08, value_gamma=0.75),
]


def opaque_runs(alpha: Image.Image, horizontal: bool) -> list[tuple[int, int]]:
    """The runs of columns (or rows, if not `horizontal`) that have opaque pixels.

    :param alpha: The alpha channel of the sheet.
    :param horizontal: Whether to look at the columns, which are found left to right; otherwise at the rows.
    :returns: The first and one past the last column (or row) of each run, in order.
    """
    length = alpha.width if horizontal else alpha.height
    runs: list[tuple[int, int]] = []
    start = None
    for i in range(length + 1):
        box = (i, 0, i + 1, alpha.height) if horizontal else (0, i, alpha.width, i + 1)
        opaque = i < length and alpha.crop(box).getbbox() is not None
        if opaque and start is None:
            start = i
        elif not opaque and start is not None:
            runs.append((start, i))
            start = None
    return runs


def recolor(image: Image.Image, gem: Gem) -> Image.Image:
    """The RGBA image with its colors changed as `gem` says; the alpha is kept."""
    hue, saturation, value = image.convert("RGB").convert("HSV").split()
    hue = hue.point(lambda h: (h + gem.hue_shift) % 256)
    saturation = saturation.point(lambda s: min(255, round(s * gem.saturation)))
    value = value.point(lambda v: round(255 * (v / 255) ** gem.value_gamma))
    result = Image.merge("HSV", (hue, saturation, value)).convert("RGBA")
    result.putalpha(image.getchannel("A"))
    return result


def icon(gem_image: Image.Image, size: int, fill: float) -> Image.Image:
    """The gem cut out of `gem_image` and scaled into the middle of a transparent square of `size` pixels.

    :param fill: The width or height of the gem, whichever is larger, as a fraction of `size`.
    """
    cropped = gem_image.crop(gem_image.getchannel("A").getbbox())
    # A square around the gem that is scaled to the icon, so that the gem fills `fill` of it.
    side = round(max(cropped.size) / fill)
    square = Image.new("RGBA", (side, side))
    square.paste(cropped, ((side - cropped.width) // 2, (side - cropped.height) // 2))
    # Pillow scales RGBA with premultiplied alpha, so the transparent pixels don't bleed their color into the edges.
    scaled = square.resize((size, size), Image.Resampling.LANCZOS)
    if size > side:
        scaled = scaled.filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))
    return scaled


def main() -> int:
    """Makes the icons from the sheet given on the command line.

    :returns: The exit code: 0 on success, 1 if the sheet doesn't show three columns and two rows of gems.
    :raises SystemExit: With code 2 if the arguments are wrong.
    :raises OSError: If the sheet can't be read or an icon can't be written.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("original", help="the sheet hjm-6_grades_of_gems_alpha.png")
    parser.add_argument("output_dir", type=Path, help="directory to write the icons to")
    parser.add_argument("--size", type=int, default=64, help="width and height of the icons, pixels")
    args = parser.parse_args()
    if args.size < 4:
        parser.error("the size must be at least 4 pixels")

    with Image.open(args.original) as original:
        sheet = original.convert("RGBA")
    alpha = sheet.getchannel("A")
    columns = opaque_runs(alpha, horizontal=True)
    rows = opaque_runs(alpha, horizontal=False)
    if len(columns) != 3 or len(rows) != 2:
        print(f"expected 3 columns and 2 rows of gems, found {len(columns)} and {len(rows)}", file=sys.stderr)
        return 1

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for gem in GEMS:
        (left, right), (top, bottom) = columns[gem.column], rows[gem.row]
        gem_image = recolor(sheet.crop((left, top, right, bottom)), gem)
        for label, fill in (("large", LARGE_FILL), ("small", SMALL_FILL)):
            path = args.output_dir / f"{gem.name}-{label}.png"
            result = icon(gem_image, args.size, fill)
            # A new image from the pixels alone, so that no metadata of the original is kept.
            Image.frombytes("RGBA", result.size, result.tobytes()).save(path, format="PNG", optimize=True)
            print(f"{path}: {right - left}x{bottom - top} at ({left}, {top}) -> {fill:.0%} of {args.size}x{args.size}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
