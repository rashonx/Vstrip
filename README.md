# Rashon Vocal Sitter

A sidechain plugin (VST3 for Windows) that carves space in your beat for the vocal, then
blends them back together, level-matched. Built with [JUCE](https://juce.com).

![Rashon Vocal Sitter](docs/screenshot.png)

## How it works

Works like a track spacer: put it on the **beat** track and feed the **vocal** into its
sidechain input. A bank of 32 filters across the spectrum dips the beat in each band by
how loud the vocal is in that band, as it happens. Zero latency. The output is the
carved beat; the vocal stays on its own track.

## Controls

| Control | What it does |
|---|---|
| **Amount** | How much space to carve out of the beat (up to 24 dB per band) |
| Attack / Release | How fast the beat ducks and comes back |
| Low Cut / High Cut | Only carve between these frequencies |
| Stereo / Mid / Side | Carve the whole beat, only its centre, or only its sides |
| Auto gain | Keeps the beat as loud as it was before carving |
| Output | Final level |

The display shows 32 bars: the beat's level in each band (blue), the part being carved
out (red), and the vocal (gold marks), plus the carve depth per band along the bottom.

## Getting the plugin

Every push to GitHub builds it: open the **Actions** tab, click the latest **Build plugin** run,
download **RashonVocalSitter-Windows**, and follow `INSTALL.txt`.

## Building yourself

Needs CMake 3.22+ and Visual Studio 2022 or newer. JUCE downloads automatically.

```bash
cmake -B build
cmake --build build --config Release
```

Tests (Linux/macOS): `cmake -B build -DSITTER_BUILD_TESTS=ON && cmake --build build --target SitterSmokeTest && ./build/SitterSmokeTest`

## Code layout

- `Source/BandCarver.*` - the 32-band zero-latency carving engine
- `Source/PluginProcessor.*` - signal flow, auto gain, parameters
- `Source/PluginEditor.*` - the window and live carve display
- `Source/Artwork.h` - 3D title and studio mic drawing
- `Tests/` - audio smoke test and screenshot tool

## License

JUCE 8+ is dual-licensed (AGPLv3 or commercial). Fine for personal use; check JUCE's terms
before selling or distributing closed-source.
