# Freesound: Footsteps On Metal

The sound [Footsteps On Metal](https://freesound.org/people/IENBA/sounds/834029/) by IENBA, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `834029__ienba__footsteps-on-metal.ogg`, used as footsteps on metal surfaces, see
  `assets/textures/metal/`.
- **Processing**: the original `834029__ienba__footsteps-on-metal.wav` (not in the repo) was converted to OGG Vorbis, at
  the same speed, with:

```sh
ffmpeg -i 834029__ienba__footsteps-on-metal.wav -map_metadata -1 -c:a libvorbis -q:a 5 834029__ienba__footsteps-on-metal.ogg
```
