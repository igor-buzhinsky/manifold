# Portraits

Portraits of characters, 240 x 320 PNG (3:4), showing the head and chest. They are grouped by who they may be used
for:

- `male/`: men.
- `female/`: women.
- `any/`: unknown or indeterminate gender, such as skeletons.

Inside each group, the files of each source are in a directory of their own, with a `README.md` that tells where they
come from, their license, which files were taken and how they were processed. A source whose portraits are in several
groups has such a directory in each of them, each listing only its own files.

The portraits were cropped from the originals with `scripts/crop_portrait.py`; each README lists the arguments, so
that a crop can be redone or changed. Each file is named after the object's number in its museum, its artist and its
title.
