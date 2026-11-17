# Freesound: wood step.mp3

The sound [wood step.mp3](https://freesound.org/people/saha213131/sounds/689416/) by saha213131, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `689416-step-01.ogg` to `689416-step-05.ogg`, single footsteps on wood surfaces (see
  `assets/textures/wood/`): the 5 clearest steps of the recording; the creaks and taps between them are left out.
- **Processing**: cut from the original `689416__saha213131__wood-step.mp3` (not in the repo) with
  `scripts/cut_sounds.py`, which gives each step a short fade in and out, scales it so that its loudest sample is at -6
  dBFS, and encodes it as OGG Vorbis. The times are seconds in the original. From the repo root:

```sh
scripts/cut_sounds.py 689416__saha213131__wood-step.mp3 \
    assets/sounds/footsteps/wood/freesound-org/689416-step \
    0.11-0.31 1.72-2.01 3.41-3.64 4.91-5.11 10.34-10.64
```
