# Freesound: wood step.mp3

The sound [wood step.mp3](https://freesound.org/people/saha213131/sounds/689416/) by saha213131, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `689416__saha213131__wood-step.ogg`, used as footsteps on wood surfaces, see `assets/textures/wood/`.
- **Processing**: the original `689416__saha213131__wood-step.mp3` (not in the repo) was converted to OGG Vorbis and
  sped up 1.5x without changing the pitch, with:

```sh
ffmpeg -i 689416__saha213131__wood-step.mp3 -af atempo=1.5 -map_metadata -1 -c:a libvorbis -q:a 5 689416__saha213131__wood-step.ogg
```
