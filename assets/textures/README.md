# Textures

The game picks the texture of a level at random among the PNG files in the four directories below, at any depth, see
`find_level_textures()`; files elsewhere are not used. The directory of the texture tells what the level's surfaces are
made of (`TextureKind`), which picks the sound of footsteps from `assets/sounds/footsteps/`:

- `grass/`: grass.
- `wood/`: wooden planks and boards.
- `metal/`: metal plates.
- `stone/`: everything else that is hard underfoot: bricks, tiles, stone, plaster and dirt.

Inside each group, the files of each pack are in a directory of their own, with the pack's `License.txt` and a
`README.md` that tells where they come from, their license, which files were taken and how they were processed. A
pack whose textures are in several groups has such a directory in each of them, each listing only its own files.
