# Freesound: 6 footsteps grass

The sound [6 footsteps grass](https://freesound.org/people/whothefuckisjojo/sounds/673305/) by whothefuckisjojo, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `673305__whothefuckisjojo__6-footsteps-grass.ogg`, used as footsteps on grass surfaces, see
  `assets/textures/grass/`.
- **Processing**: the original `673305__whothefuckisjojo__6-footsteps-grass.mp3` (not in the repo) was converted to OGG
  Vorbis, at the same speed, with:

```sh
ffmpeg -i 673305__whothefuckisjojo__6-footsteps-grass.mp3 -map_metadata -1 -c:a libvorbis -q:a 5 673305__whothefuckisjojo__6-footsteps-grass.ogg
```
