# The Metropolitan Museum of Art

Portraits of women from the [Open Access](https://www.metmuseum.org/hubs/open-access) collection of The
Metropolitan Museum of Art, New York.

- **License**: CC0 1.0 Universal (public domain), as the Met's Open Access policy states for images of public-domain
  works. Credit is not required.
- **Processing**: the full-size image of each object was downloaded through the Met's collection API and
  cropped to 3:4 with `poetry run python crop_portrait.py ORIGINAL OUTPUT CENTER_X CENTER_Y HEIGHT` in `scripts/`,
  which also scales it to 240 x 320. The arguments are given below, in pixels of the original.

## Files

| File | Object | Crop arguments |
| ---- | ------ | -------------- |
| `437945-wissing-portrait-of-a-woman.png` | Willem Wissing, [*Portrait of a Woman*](https://www.metmuseum.org/art/collection/search/437945), ca. 1687, oil on canvas | 1470 959 1318 |
| `906167-renaldi-mughal-woman.png` | Francesco Renaldi, [*Portrait of a Mughal Woman*](https://www.metmuseum.org/art/collection/search/906167), 1787, oil on canvas | 1433 1320 1400 |
| `287794-unknown-indian-woman.png` | Unknown photographer, [*Bust Portrait of an Indian Woman*](https://www.metmuseum.org/art/collection/search/287794), 1850s, albumen silver print | 970 1228 1551 |
