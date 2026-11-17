# Freesound: Footsteps On Metal

The sound [Footsteps On Metal](https://freesound.org/people/IENBA/sounds/834029/) by IENBA, from
[Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `834029-step-01.ogg` to `834029-step-10.ogg`, single footsteps on metal surfaces (see
  `assets/textures/metal/`): all 10 steps of the recording.
- **Processing**: cut from the original `834029__ienba__footsteps-on-metal.wav` (not in the repo) with
  `scripts/cut_sounds.py`, which gives each step a short fade in and out, scales it so that its loudest sample is at -6
  dBFS, and encodes it as OGG Vorbis. The times are seconds in the original. From the repo root, with the original
  downloaded to `ORIGINALS`:

```sh
scripts/cut_sounds.py "$ORIGINALS"/834029__ienba__footsteps-on-metal.wav \
    assets/sounds/footsteps/metal/freesound-org/834029-step \
    0.02-0.33 0.59-0.92 1.10-1.31 1.60-1.84 2.18-2.44 2.69-2.96 3.26-3.53 3.75-4.04 4.41-4.72 4.98-5.31
```
