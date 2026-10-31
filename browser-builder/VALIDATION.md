# Browser builder validation

The builder converts PC game data locally and packages the normal v1.2.0
runtime. The static website contains the port executable, conversion tools,
corresponding tool sources and asset recipes. It contains no retail game data.

## Runtime and package checks

Site preparation checks the 64 MiB original-3DS configuration, disabled
benchmarking and profiling, accepted visual features, stereo submission policy,
Title ID `000400000f3a1100` and version 1.2.0 (1056).

The independent CIA verifier checks:

- Executable identity against a native package from the same runtime.
- All 88 prepared world files and their admission manifest.
- Every unchanged PC asset against the original source bytes.
- Converted assets by exact hash, or identical decoded pixels and unchanged
  non-image chunks when PNG compression differs.
- The complete native texture bundle, selected dialogue archive, language
  marker, cinematic headers and recorded full-decode results.
- Container and filesystem hashes with `ctrtool --verify`.

The current English, French and Spanish data paths use the same executable.
The full browser build from the English installed folder with replacement
Ogg/Theora publisher clips passes conversion and independent package checks.
Required gameplay data and unsupported or corrupt movies still fail validation.

## Language inputs

French validation uses all three original disc images. Cabinet extraction checks
its original MD5 records; a separate native extraction supplies the comparison
files. Spanish uses the installed data extracted from its original cabinets.
English also supports the complete installed folder from the original PC image.

Changing language or source data changes checkpoint identity. A build can reuse
only completed stages whose stored files pass their size and hash checks.

## Scope

These are desktop browser conversion and package-integrity checks. Physical
Old 3DS tests exercise the normal executable and bundled world-residency path;
package verification alone does not establish installation or gameplay results.
New 3DS camera-input emulation does not establish physical accessory support.

Earlier macOS Safari full-build and large-file export checks cover the existing
packaging/export path. The current language packages use Chromium. End-to-end
builds on physical iPhone and Android devices remain unverified.
