#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
package_root="dist/GATE-VERB-1.0.0-macOS-Universal"
plugin_path="$build_dir/GateVerb_artefacts/Release/VST3/GATE-VERB.vst3"
juce_source="$build_dir/_deps/juce-src"

if [[ ! -d "$plugin_path" || ! -f "$juce_source/LICENSE.md" ]]; then
    echo "The built VST3 or matching JUCE source is missing." >&2
    exit 1
fi
if [[ -e "$package_root" ]]; then
    echo "Package directory already exists; use a clean dist directory." >&2
    exit 1
fi

mkdir -p "$package_root/Source" "$package_root/Licences"
ditto "$plugin_path" "$package_root/GATE-VERB.vst3"
cp docs/USER_GUIDE_DE.md "$package_root/START_HERE_DE.md"
cp docs/BUILD.md "$package_root/Source/BUILD.md"
cp THIRD_PARTY_NOTICES.md "$package_root/THIRD_PARTY_NOTICES.md"
cp LICENSE "$package_root/Licences/GATE-VERB-MIT-LICENSE.txt"
cp COPYING-AGPL-3.0.txt "$package_root/Licences/COPYING-AGPL-3.0.txt"
cp "$juce_source/LICENSE.md" "$package_root/Licences/JUCE-LICENSE.md"

# Include corresponding source with the combined AGPL build, including JUCE's
# original third-party notices. There is no dependency on an expiring URL.
git archive --format=zip --output="$package_root/Source/GATE-VERB-source.zip" HEAD
tar -czf "$package_root/Source/JUCE-8.0.17-security-source.tar.gz" \
    -C "$build_dir/_deps" juce-src

git rev-parse HEAD > "$package_root/SOURCE_COMMIT.txt"
{
    echo "GATE-VERB 1.0.0"
    echo "macOS VST3: x86_64 + arm64"
    echo "Build: Release"
    echo "JUCE: 8.0.17-security"
    echo "Minimum deployment target: macOS 10.15 Intel / macOS 11 Apple Silicon"
    echo "Signature: local ad-hoc; no Developer ID signature or notarization"
    echo "Source repository: https://github.com/OliverUllmann/s_n_r_gate-verb"
    echo "Source commit: $(git rev-parse HEAD)"
    echo "Xcode: $(xcodebuild -version | tr '\n' ' ')"
} > "$package_root/BUILD_INFO.txt"

if [[ -f test-output/GATE-VERB-UI.png ]]; then
    cp test-output/GATE-VERB-UI.png "$package_root/GATE-VERB-UI.png"
fi

ditto -c -k --sequesterRsrc --keepParent "$package_root" "$package_root.zip"
shasum -a 256 "$package_root.zip" > "$package_root.zip.sha256"
echo "Created $package_root.zip"
