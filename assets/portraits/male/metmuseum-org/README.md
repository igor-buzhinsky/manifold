# The Metropolitan Museum of Art

Portraits of men from the [Open Access](https://www.metmuseum.org/hubs/open-access) collection of The Metropolitan
Museum of Art, New York.

- **License**: CC0 1.0 Universal (public domain), as the Met's Open Access policy states for images of public-domain
  works. Credit is not required.
- **Processing**: the full-size image of each object was downloaded through the Met's collection API and
  cropped to 3:4 with `poetry run python crop_portrait.py ORIGINAL OUTPUT CENTER_X CENTER_Y HEIGHT` in `scripts/`,
  which also scales it to 240 x 320. The arguments are given below, in pixels of the original.

## Files

| File | Object | Crop arguments |
| ---- | ------ | -------------- |
| `11058-healy-portrait-of-the-artist.png` | George P. A. Healy, [*Portrait of the Artist*](https://www.metmuseum.org/art/collection/search/11058), 1851, oil on canvas | 956 975 1638 |
| `856295-biard-study-of-a-man.png` | François-Auguste Biard, [*Bust-Length Study of a Man*](https://www.metmuseum.org/art/collection/search/856295), 1848, oil on paper | 1934 1734 2934 |
| `406437-allen-ginter-clam-fish.png` | Allen & Ginter, [*Clam Fish, Warm Springs*](https://www.metmuseum.org/art/collection/search/406437), from the American Indian Chiefs series of cigarette cards, 1888, color lithograph | 516 791 1008 |
