# GATE-VERB

**by studiobro.net for Sound&Recording Magazin**

[![Build macOS VST3](https://github.com/OliverUllmann/s_n_r_gate-verb/actions/workflows/build-macos.yml/badge.svg)](https://github.com/OliverUllmann/s_n_r_gate-verb/actions/workflows/build-macos.yml)

A stereo reverb with a drawable, tempo-synchronised amplitude envelope for the reverb tail. Written in C++17 with JUCE. This open-source project accompanies an article in Sound&Recording magazine; the original project code is available for readers to study and adapt under the existing MIT licence.

## Download the plug-in

1. Open [GitHub Actions](https://github.com/OliverUllmann/s_n_r_gate-verb/actions/workflows/build-macos.yml).
2. Open the latest successful build on `main`.
3. Download the macOS VST3 artifact in **Artifacts**. You must be signed into GitHub to download Actions artifacts.
4. Extract the downloaded archive, then extract the enclosed product ZIP if present. Copy the complete `GATE-VERB.vst3` bundle into `~/Library/Audio/Plug-Ins/VST3/`.
5. Rescan plug-ins or restart your VST3 host.

The build produces a universal macOS VST3 for Intel and Apple Silicon. The compatibility target is macOS 10.15 or later on Intel and macOS 11 or later on Apple Silicon. Developer ID signing and notarisation are left to the distributor. The CI package uses only an ad-hoc signature for development; it is not a notarised public installer.

The build also provides its corresponding source, licence notices and an image of the actual plug-in editor. See [build instructions](docs/BUILD.md) for local builds and signing.

## The signal path

`Input → IN → dry/wet split → Reverb → drawable Gate → WET/DRY → OUT`

The gate multiplies the **wet reverb output**, so closing the gate does not stop or clear the internal reverb. An existing tail can therefore reappear when the gate opens. The dry branch bypasses the reverb and gate. The mix is linear: at 0% only dry is heard; at 100% only the gated reverb is heard.

## Features

- Original native vector interface with a large interactive envelope display, actual audio/transport-driven animation, physical-style knobs and input/output meters.
- Freehand drawing with interpolated strokes, or stepped editing on a 4/8/16/32/64-cell grid.
- Open, Pulse, Duck, Rise, Fall, Sine and Steps starting shapes; curve undo/redo in the editor.
- Tempo sync from the host PPQ position, including triplet and dotted periods, or a free-running 0.05–20 Hz clock.
- Gate depth, phase and smoothing; independent gate enable.
- Eight-line modulated feedback-delay reverb with input diffusion, pre-delay, decay, room size, damping, wet low/high cuts and stereo width.
- Twelve factory presets, user preset save/load and complete DAW session recall of the parameters and drawn curve.
- Mono and stereo processing; smooth continuous parameters; fixed-memory audio processing after preparation.
- Offline rendering uses the same DSP path as real-time playback. The editor does not drive the effect.

## Bedienung

Die vollständige deutsche Anleitung mit Parametern, Gate-Zeichnen, Presets und Installation steht in [docs/USER_GUIDE_DE.md](docs/USER_GUIDE_DE.md).

**Schnellstart:** Lade `01 - Gated Bloom`, starte den DAW-Transport, stelle `SYNC` auf `1/4` und zeichne eine eigene Kurve. Oben bedeutet volle Halllautstärke, unten geschlossener Hall. Für einen Send-Kanal wähle 100% WET/DRY oder eines der Send-Presets.

## Source layout

| File | Purpose |
| --- | --- |
| `Source/DspEngine.h/.cpp` | JUCE-independent audio engine, reverb, gate clock and smoothing |
| `Source/PluginProcessor.h/.cpp` | JUCE parameters, host transport, audio buses, presets and session state |
| `Source/PluginEditor.h/.cpp` | Native interface and gate drawing interaction |
| `Tests/` | Audio invariants, clock/render consistency, state roundtrips and editor snapshot |
| `.github/workflows/build-macos.yml` | Universal macOS build, checks and downloadable artifacts |

## Licensing

The repository's original MIT [LICENSE](LICENSE) remains in place for GATE-VERB's own code. JUCE and its bundled dependencies have separate terms. The CI distribution is prepared for the **JUCE AGPLv3 open-source route**, with the complete corresponding source and notices provided alongside the binary. A proprietary distribution may require a commercial JUCE licence; the MIT licence for this project's original code does not remove JUCE's requirements.

Read [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [COPYING-AGPL-3.0.txt](COPYING-AGPL-3.0.txt). No JUCE account, licence key or Apple signing secret is needed for the configured open-source CI build.

## Version and testing

This is an initial implementation. CI checks support the declared behavior and build integrity; listening tests and compatibility testing in the intended DAWs and on actual target Macs remain part of release qualification. An automated editor snapshot is a render of the real JUCE interface, not a separate mock-up.
