# Textures

The game picks the texture of a level at random among all PNG files here, at any depth, see `find_textures()`. The
files are grouped by what the surface is, so that things that depend on it, such as the sound of footsteps, can tell:

- `grass/`: grass.
- `wood/`: wooden planks and boards.
- `metal/`: metal plates.
- `stone/`: everything else that is hard underfoot: bricks, tiles, stone, plaster and dirt.

Inside each group, the files of each pack are in a directory of their own, with the pack's `License.txt` and a
`README.md` that tells where they come from, their license, which files were taken and how they were processed. A
pack whose textures are in several groups has such a directory in each of them, each listing only its own files.
