# Shimmer glitter magic

The sound [Shimmer glitter magic](https://opengameart.org/content/shimmer-glitter-magic) on OpenGameArt, used as the
casting sound of spells.

- **Author**: The Berklee College of Music, as the page states (submitted by qubodup).
- **License**: [CC-BY 3.0](https://creativecommons.org/licenses/by/3.0/), as stated on the sound's page. OpenGameArt
  gives no license file with the download. The file was changed as described below.
- **File taken**: `shimmer_1.flac`, 0.66 s, mono, 44.1 kHz. It is not kept here.
- **Processing**: encoded as OGG Vorbis (`shimmer_1.ogg`), since the game reads only OGG Vorbis, without its
  metadata, and otherwise unchanged (its loudest sample is at -3 dBFS, near those of the other sounds):

```sh
ffmpeg -i shimmer_1.flac -map_metadata -1 -c:a libvorbis -q:a 6 shimmer_1.ogg
```
