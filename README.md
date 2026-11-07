# Rashon Vocal Sitter

A sidechain plugin (VST3 for Windows) that carves space in your beat for the vocal, then
blends them back together, level-matched. Built with [JUCE](https://juce.com).

![Rashon Vocal Sitter](docs/screenshot.png)

## How it works

Put it on the **vocal** track and feed the **beat** into its sidechain input. Many times a
second it looks at which frequencies the vocal is using and turns the beat down only at
those frequencies, only while the vocal is there. The output is the vocal plus the carved beat.

## Controls

| Section | Control | What it does |
|---|---|---|
| Carve | **Carve Amount** | How much of the beat gets carved out (0–100%, up to 18 dB) |
| | Low Cut / High Cut | Only carve between these frequencies |
| | Release | How fast the beat comes back after the vocal stops |
| Vocal EQ | 7 faders | 63 Hz, 160 Hz, 400 Hz, 1 kHz, 2.5 kHz, 6.3 kHz, 16 kHz (±12 dB) |
| Mix | Vocal / Beat | Balance between vocal and beat |
| | Beat to output | Include the carved beat in the output |
| | Auto gain | Keeps the output as loud as the input, so carving and EQ don't change the level |
| | Output | Final level |

The display shows the beat (blue), what's being carved out (red), the vocal (gold), and the
carve depth across the frequency range. Areas outside Low Cut / High Cut are dimmed.

The plugin adds 2048 samples (~43 ms) of latency, which your DAW compensates automatically.

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

- `Source/SpectralCarver.*` - the FFT carving engine
- `Source/PluginProcessor.*` - signal flow, vocal EQ, mix, auto gain, parameters
- `Source/PluginEditor.*` - the window and live carve display
- `Source/Artwork.h` - 3D title and studio mic drawing
- `Tests/` - audio smoke test and screenshot tool

## License

JUCE 8+ is dual-licensed (AGPLv3 or commercial). Fine for personal use; check JUCE's terms
before selling or distributing closed-source.
