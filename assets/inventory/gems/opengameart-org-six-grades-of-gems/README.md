# Six grades of gems

Icons of gems made from [Six grades of gems](https://opengameart.org/content/six-grades-of-gems) on OpenGameArt.

- **Author**: Hansjörg Malthaner ([Varkalandar](http://opengameart.org/users/varkalandar)), who asks to be credited
  with his name and this link.
- **License**: [CC-BY 3.0](https://creativecommons.org/licenses/by/3.0/). The work is also offered under CC-BY-SA 3.0,
  GPL 2.0, GPL 3.0 and OGA-BY 3.0; it is used here under CC-BY 3.0 alone. The files were changed as described below.
- **File taken**: `hjm-6_grades_of_gems_alpha.png`, 210 x 140 pixels, six red gems in three columns and two rows on a
  transparent background. It is not kept here.
- **Processing**: `poetry run python make_gem_icons.py hjm-6_grades_of_gems_alpha.png
  ../assets/inventory/gems/opengameart-org-six-grades-of-gems` in `scripts/`. Each gem of the game takes another gem
  of the sheet, except the cut-off one at the bottom right, and is recolored: its hue is turned, except for the ruby,
  and the diamond loses most of its saturation. Each is cut out and scaled with Lanczos interpolation into the middle
  of two transparent squares of 64 x 64: `GEM-large.png`, which it fills to 90% of its width or height, and
  `GEM-small.png`, to 57%, so that the small gem has a quarter of the volume of the large one. The script tells the
  hue, saturation and brightness of each gem.

## Files

| Gem | Gem of the sheet | Files |
| --- | ---------------- | ----- |
| Ruby | top left, original color | `ruby-large.png`, `ruby-small.png` |
| Emerald | top middle | `emerald-large.png`, `emerald-small.png` |
| Sapphire | top right | `sapphire-large.png`, `sapphire-small.png` |
| Amethyst | bottom left | `amethyst-large.png`, `amethyst-small.png` |
| Diamond | bottom middle | `diamond-large.png`, `diamond-small.png` |
