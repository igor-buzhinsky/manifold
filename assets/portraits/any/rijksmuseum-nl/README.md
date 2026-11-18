# Rijksmuseum

Portraits from the collection of the [Rijksmuseum](https://www.rijksmuseum.nl/), Amsterdam.

- **License**: public domain, marked with the
  [Public Domain Mark 1.0](https://creativecommons.org/publicdomain/mark/1.0/) on the object's page. Credit is not
  required.
- **Processing**: the full-size image of each object was downloaded through the Rijksmuseum's IIIF image service and
  cropped to 3:4 with `poetry run python crop_portrait.py ORIGINAL OUTPUT CENTER_X CENTER_Y HEIGHT` in `scripts/`,
  which also scales it to 240 x 320. The arguments are given below, in pixels of the original.

## Files

| File | Object | Crop arguments |
| ---- | ------ | -------------- |
| `RP-F-00-1017-huth-human-skeleton.png` | Carl Huth, [*Enscenering van skeletten van een mens en van een gorilla aan tafel*](https://www.rijksmuseum.nl/en/collection/object/Enscenering-van-skeletten-van-een-mens-en-van-een-gorilla-aan-tafel--9054f7fda289827873125c41f7ef0601) (staged skeletons of a human and a gorilla at a table), RP-F-00-1017, 1878, photograph. Only the human skeleton on the left is taken. | 506 1275 480 |
