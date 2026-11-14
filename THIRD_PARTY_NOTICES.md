# Third-party software and source availability

## GATE-VERB source

The original GATE-VERB source in this repository is covered by the existing MIT
licence in `LICENSE`, copyright (c) 2026 M. Oliver Ullmann. This notice does not
replace that licence.

## JUCE 8.0.17-security

GATE-VERB is built with JUCE. JUCE modules are dual licensed under GNU Affero
General Public License version 3 (AGPLv3), or a commercial JUCE licence.

The downloadable open-source build produced by this repository uses the AGPLv3
option for JUCE. The combined executable is distributed under AGPLv3, while the
original GATE-VERB files retain their MIT licence. The package contains a copy
of AGPLv3, the matching GATE-VERB source, the full matching JUCE source, and the
build instructions. The MIT grant on the original source does not remove the
licence conditions applicable to JUCE or to the combined binary.

If you distribute a version without meeting the AGPLv3 requirements, an
appropriate commercial JUCE licence may be required. Review the current terms
for your intended distribution; the code does not require a JUCE account or
licence key to compile.

- JUCE source: https://github.com/juce-framework/JUCE/tree/8.0.17-security
- JUCE licence: https://github.com/juce-framework/JUCE/blob/8.0.17-security/LICENSE.md
- AGPLv3: https://www.gnu.org/licenses/agpl-3.0.html
- Commercial JUCE terms: https://juce.com/legal/juce-8-licence/

The build verifies the JUCE archive SHA-256:
`d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`.

JUCE contains additional dependencies. Their original copyright notices and
licences are preserved in the included JUCE source archive. The top-level
`JUCE-LICENSE.md` in the download lists these dependencies and the locations of
their licence texts. This includes the VST3 SDK, image codecs, font shaping,
Unicode and compression code used by the framework. JUCE's bundled VST3 SDK
uses the MIT licence.

## Validation tools

The GitHub Actions workflow uses Tracktion pluginval 1.0.4 for validation. It is
a build/test tool and is not linked into, or included in, the GATE-VERB download.
Project and licence: https://github.com/Tracktion/pluginval
