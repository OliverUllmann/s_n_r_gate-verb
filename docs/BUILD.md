# Building GATE-VERB

## Download the compiled macOS plug-in

Open [GitHub Actions](https://github.com/OliverUllmann/s_n_r_gate-verb/actions/workflows/build-macos.yml),
choose a successful **Build macOS VST3** run, and download the
**GATE-VERB-macOS-Universal** artifact. The artifact contains a ZIP with
`GATE-VERB.vst3`. Extract the ZIP before installing the entire `.vst3` bundle.

The workflow runs after pushes to `main`, for pull requests, and manually from
**Run workflow**. It compiles a Release VST3 with `x86_64` and `arm64` slices on
`macos-15-intel`. No Apple account or signing secrets are needed. The local ad-hoc
signature is checked before packaging; this is not a Developer ID signature or
notarization. The owner handles distribution signing.

The deployment targets are macOS 10.15 for Intel and macOS 11 for Apple Silicon.
These are build targets, not a claim that every DAW/OS combination has been tested.
CI currently executes the tests on the Intel slice and verifies that both slices
are present. Audio testing in a real DAW on each supported Mac remains useful.

## Build locally on a Mac

Install Xcode with its command line tools and CMake 3.22 or later. The build
downloads a fixed JUCE 8.0.17-security archive and verifies its SHA-256 hash.

```sh
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release --target GateVerb_VST3 GateVerbDspTests GateVerbProcessorTests --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

The plugin is written to:

```text
build/GateVerb_artefacts/Release/VST3/GATE-VERB.vst3
```

To use the corresponding JUCE source shipped in the download instead of fetching
it again, extract the JUCE archive and add this option when configuring:

```sh
-DFETCHCONTENT_SOURCE_DIR_JUCE=/absolute/path/to/juce-src
```

For an actual editor screenshot, run:

```sh
./build/GateVerbProcessorTests --snapshot /absolute/path/GATE-VERB-UI.png
```

The snapshot is rendered by the real JUCE editor after processing a synthetic
rhythmic input through the real processor.

## Automated checks

- `GateVerbDspTests` checks silence, dry-path gain, predelay, a decaying impulse
  response, wet-only gating, gate bypass, host sync, negative positions, stopped
  transport, free running, block sizes from 1 to 4096, high-decay stability at
  44.1/48/96 kHz, reset, and invalid floating-point input.
- `GateVerbProcessorTests` checks all 18 parameters and 256 gate points through
  host-state save/restore and user preset save/load, rejects incomplete states,
  exercises mono/stereo and zero/variable/oversized host buffers, and re-prepares
  at a different sample rate.
- The CI build validates the final `.vst3` bundle with Tracktion pluginval 1.0.4
  at strictness level 5, verifies both Mach-O architectures, and checks the local
  signature. Logs are available in the validation artifact.

The dependency-free audio checks can also run on Linux/Windows:

```sh
cmake -S . -B build-dsp -DGATEVERB_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

## Source and licences

See `THIRD_PARTY_NOTICES.md`. This repository preserves the original MIT licence
for GATE-VERB code. The combined open-source JUCE build is distributed under
AGPLv3. Each downloadable package includes the exact GATE-VERB source revision,
the matching complete JUCE source and its third-party licence notices, and a copy
of AGPLv3. A commercial JUCE licence may be needed for a distribution that does
not follow the AGPLv3 conditions.

JUCE is pinned to `8.0.17-security` with SHA-256
`d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`.
The pluginval download and the GitHub actions are also pinned.
