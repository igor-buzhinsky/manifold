# Freesound: 6 footsteps grass

The sound [6 footsteps grass](https://freesound.org/people/whothefuckisjojo/sounds/673305/) by whothefuckisjojo, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `673305-step-01.ogg` to `673305-step-06.ogg`, single footsteps on grass surfaces (see
  `assets/textures/grass/`): all 6 steps of the recording.
- **Processing**: cut from the original `673305__whothefuckisjojo__6-footsteps-grass.mp3` (not in the repo) with
  `scripts/cut_sounds.py`, which gives each step a short fade in and out, scales it so that its loudest sample is at -6
  dBFS, and encodes it as OGG Vorbis. The times are seconds in the original. From the repo root, with the original
  downloaded to `ORIGINALS`:

```sh
scripts/cut_sounds.py "$ORIGINALS"/673305__whothefuckisjojo__6-footsteps-grass.mp3 \
    assets/sounds/footsteps/grass/freesound-org/673305-step \
    0.03-0.38 0.38-0.74 0.74-1.14 1.14-1.55 1.55-1.96 1.96-2.20
```
