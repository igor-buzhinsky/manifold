#!/usr/bin/env python3
r"""Crops a 3:4 portrait out of a picture and scales it to a fixed size, e.g. a head and chest from a painting.

The crop is given by its center and height in pixels of the original, which can be read off any image viewer;
its width is three quarters of its height. The result is written as PNG in sRGB without metadata: a picture with
a color profile is converted to sRGB first. Needs Pillow, so run it with ``poetry run`` from this folder, not with
``poetry -C scripts run`` from elsewhere: that also runs it in this folder, so relative paths in the arguments
would be wrong.

Usage::

    cd scripts
    poetry run python crop_portrait.py ORIGINAL OUTPUT.png CENTER_X CENTER_Y HEIGHT

For example, to crop a box 1638 pixels high around (956, 975) and scale it to 240 x 320::

    poetry run python crop_portrait.py ~/Downloads/met-11058.jpg \
        ../assets/portraits/male/metmuseum-org/11058-healy-portrait-of-the-artist.png 956 975 1638
"""

import argparse
import io
import sys

from PIL import Image, ImageCms, ImageOps

ASPECT = 3 / 4  # Width divided by height.


def crop_box(center_x: int, center_y: int, height: int) -> tuple[int, int, int, int]:
    """The 3:4 box with the given center and height.

    :returns: The left, top, right and bottom edges of the box, in pixels, as Pillow's ``crop`` wants them.
    """
    width = round(height * ASPECT)
    left = center_x - width // 2
    top = center_y - height // 2
    return left, top, left + width, top + height


def to_srgb(image: Image.Image) -> Image.Image:
    """The picture as RGB in the sRGB color space.

    A picture with an embedded color profile, such as Adobe RGB, is converted from it, so that its colors look the
    same once the profile is dropped. A picture without one is taken to be sRGB already.
    """
    icc_profile = image.info.get("icc_profile")
    if not icc_profile:
        return image.convert("RGB")
    source = ImageCms.ImageCmsProfile(io.BytesIO(icc_profile))
    converted = ImageCms.profileToProfile(image, source, ImageCms.createProfile("sRGB"), outputMode="RGB")
    assert converted is not None  # Only None when converting in place.
    return converted


def main() -> int:
    """Crops and scales the picture given on the command line.

    :returns: The exit code: 0 on success, 1 if the box doesn't fit into the picture.
    :raises SystemExit: With code 2 if the arguments are wrong.
    :raises OSError: If the picture can't be read or the result can't be written.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("original", help="the picture to crop, in any format Pillow reads")
    parser.add_argument("output", help="path of the PNG file to write")
    parser.add_argument("center_x", type=int, help="horizontal center of the crop, pixels from the left")
    parser.add_argument("center_y", type=int, help="vertical center of the crop, pixels from the top")
    parser.add_argument("height", type=int, help="height of the crop, pixels")
    parser.add_argument("--output-height", type=int, default=320, help="height of the result, pixels")
    args = parser.parse_args()
    if args.height < 4 or args.output_height < 4:
        parser.error("heights must be at least 4 pixels")

    with Image.open(args.original) as original:
        # Turns the picture as its EXIF orientation says, so that the coordinates match what viewers show.
        image = to_srgb(ImageOps.exif_transpose(original))
    left, top, right, bottom = crop_box(args.center_x, args.center_y, args.height)
    if left < 0 or top < 0 or right > image.width or bottom > image.height:
        print(
            f"the box {right - left}x{bottom - top} at ({left}, {top}) doesn't fit into the picture "
            f"{image.width}x{image.height}",
            file=sys.stderr,
        )
        return 1

    output_size = (round(args.output_height * ASPECT), args.output_height)
    portrait = image.crop((left, top, right, bottom)).resize(output_size, Image.Resampling.LANCZOS)
    # A new image from the pixels alone, so that no metadata of the original, such as EXIF or an ICC profile, is kept.
    Image.frombytes("RGB", portrait.size, portrait.tobytes()).save(args.output, format="PNG", optimize=True)
    print(f"{args.output}: {right - left}x{bottom - top} at ({left}, {top}) -> {output_size[0]}x{output_size[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
