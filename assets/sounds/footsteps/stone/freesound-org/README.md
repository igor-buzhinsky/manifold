# Freesound: Footsteps - Stone, Rock, Concrete, Cement

The sound [Footsteps - Stone, Rock, Concrete, Cement](https://freesound.org/people/SecureSubset/sounds/813622/) by
SecureSubset, from [Freesound](https://freesound.org/).

- **License**: CC0 1.0 Universal ([public domain](https://creativecommons.org/publicdomain/zero/1.0/)), as stated on the
  sound's page. Credit is not required. Freesound gives no license file with the download.
- **Contents**: `813622__securesubset__footsteps-stone-rock-concrete-cement.ogg`, used as footsteps on stone surfaces,
  see `assets/textures/stone/`.
- **Processing**: the original `813622__securesubset__footsteps-stone-rock-concrete-cement.wav` (not in the repo) was
  converted to OGG Vorbis and sped up 2x without changing the pitch, with:

```sh
ffmpeg -i 813622__securesubset__footsteps-stone-rock-concrete-cement.wav -af atempo=2 -map_metadata -1 -c:a libvorbis -q:a 5 813622__securesubset__footsteps-stone-rock-concrete-cement.ogg
```
