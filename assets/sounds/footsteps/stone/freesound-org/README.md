# Freesound: Footsteps - Stone, Rock, Concrete, Cement

The sound [Footsteps - Stone, Rock, Concrete, Cement](https://freesound.org/people/SecureSubset/sounds/813622/) by
SecureSubset, from [Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `813622-step-01.ogg` to `813622-step-06.ogg`, single footsteps on stone surfaces (see
  `assets/textures/stone/`): all 6 steps of the recording.
- **Processing**: cut from the original `813622__securesubset__footsteps-stone-rock-concrete-cement.wav` (not in the
  repo) with `scripts/cut_sounds.py`, which gives each step a short fade in and out, scales it so that its loudest
  sample is at -6 dBFS, and encodes it as OGG Vorbis. The times are seconds in the original. From the repo root:

```sh
scripts/cut_sounds.py 813622__securesubset__footsteps-stone-rock-concrete-cement.wav \
    assets/sounds/footsteps/stone/freesound-org/813622-step \
    0.01-0.27 1.02-1.53 2.19-2.56 3.44-3.73 4.78-5.05 6.10-6.51
```
