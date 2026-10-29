# Browser CIA builder

This static application prepares a complete v1.2 3DS installer from a PC disc
image or installed game folder selected on the visitor's device. Conversion and
packaging run locally in Web Workers. No desktop program, account or API key is
needed. Game data is never included in the website or uploaded to a server.

The application uses OPFS for temporary output. Chrome and Edge can stream the
finished CIA to a selected save location. Other browsers use an OPFS-backed File
download, without reading the whole CIA into a JavaScript buffer. Safari's
download manager bypasses service-worker download responses, so production
saving deliberately uses the File route instead.

Use a current Chrome, Edge or Safari browser and allow at least 8 GB of free
space. Keep the tab open while building and saving. A supported ISO is the
preferred phone input; folder selection depends on the browser's file picker.
Desktop end-to-end checks and mobile device checks are separate evidence.
Neither responsive desktop layout nor Mac Safari proves iPhone support.

## Source layout

- `app.mjs` handles file selection, progress, cancellation and saving.
- `worker.mjs` runs inspection, textures, v1.2 visuals, media and packaging in
  separate workers, releasing each phase's heaps before starting the next.
- `engine-conversion/` contains ISO reading and game-data preparation.
- `tools.mjs` adapts the WebAssembly tools and the texture worker pool.
- `opfs.mjs` supplies temporary storage and disk-backed makerom output.
- `checkpoints.mjs` binds reusable outputs to the selected source contents and
  builder recipe. Same-size corrupt files cannot pass checkpoint validation.
- `engine-conversion/v12-assets.py` runs the accepted seven-map visual recipe.
- `platform.mjs` limits texture workers on phones without reducing asset quality.
- `scripts/` builds the pinned tools and prepares the static website.
- `THIRD_PARTY_NOTICES.md` and `licenses/` contain tool licensing information.

The reference input is an English PC copy. Installed game folders, directly
readable PC ISO layouts and the tested Inno/FreeArc PC image layout are handled
locally. This is not a universal installer or console-disc converter. Unsupported
ISO layouts receive a request to select the installed PC folder instead. Other
languages remain unvalidated.

Website preparation verifies the frozen accepted v1.2 executable and diagnostic
settings, strips debug information into a new file, and binds the runtime,
packaging configuration, texture profile, visual recipe and processing tools to
a build ID. The original executable remains unchanged. The CIA keeps Title ID
`000400000f3a1100`, version 1.2.0 (packed value 1056), the original-3DS 64 MiB
configuration and the accepted visual feature mask 255. No game runtime source
changes are part of the browser builder.

All 201 required v1.2 outputs are checked against the accepted release. P3D
checks permit only lossless PNG compression/metadata differences when dimensions
and decoded RGBA pixels match; other chunk contents and order remain bound.
The small rounding manifest restores measured native-versus-WASM RGB rounding
differences only for exact matching source and intermediate hashes. It cannot
alter alpha, geometry or an unknown copy. The final accepted hash still applies.

Map lighting reuses exact duplicate position/normal queries within one scene.
Its bounded cache changes no ray result. The DSP cache uses the unchanged
production encoder and matches all 38 accepted clips. Cinematics retain their
existing conversion quality and complete-audio validation.

The ETC1 adapter also reuses exact 4×4 RGBA blocks in a bounded 2.5 MiB table per
worker. Full pixel comparison protects against hash collisions, and encoding
still uses the unmodified high-quality `rg_etc1` algorithm for every cache miss.
The complete texture bundle must retain the accepted SHA-256 before packaging.
See [validation evidence](VALIDATION.md) for measured runs and device limits.

## Maintainer checks

These checks use synthetic fixtures and do not require game files:

```sh
node browser-builder/tests/conversion-unit.mjs
node browser-builder/tests/iso-unit.mjs
node browser-builder/tests/local-state.mjs
python3 browser-builder/tests/v12-asset-unit.py
python3 browser-builder/tests/asset-semantics.py
python3 tools/3ds/tests/home_artwork.py
python3 tools/3ds/tests/release_workflow.py
```

The optional real-media check takes private local fixtures. Its output reports
decoded frame counts, PCM lengths and audio peaks:

```sh
node browser-builder/tests/real-media.mjs \
  --core-dir out/browser-tools/minimal-ffmpeg \
  --movie /path/to/your/movie.rmv \
  --game /path/to/your/pc-game
```

`tests/streaming-production.mjs` is an opt-in comparison of the old and bounded
memory WebAssembly packagers. It verifies container integrity and every RomFS
asset against private input hashes. The streaming path keeps NCCH scratch data
in OPFS instead of allocating the whole game in WASM. These tests can consume
several gigabytes of temporary disk space; run them only when packaging changes
require them. Prepared-data packaging proof does not prove source conversion.

`tests/local-export.html` tests 8 MiB and 1.9 GiB synthetic files, including
saved-file length and sampled sentinels. Serve it on a separate localhost origin
from a real build. It never contains game data, and its sampled checks are not a
full-file hash. The service-worker route remains in this test for comparison.

`tests/verify-v12-cia.py` independently reads a private finished CIA. It compares
all required enhancements, native textures, unchanged PC data, movie metadata,
character greeting containers and executable code against accepted inputs, then
runs `ctrtool --verify`. Supply the private paths with `--cia`, `--recipe`,
`--game`, `--reference-cia`, `--report`, `--ctrtool` and `--out`. It reads originals
without extracting a second complete game tree.

The HTML tests under `tests/` are local development harnesses. They must not be
copied into a production website. Container, conversion and emulator checks are
separate from installation and gameplay on a physical console.

## Static output

Build the normal runtime with `tools/3ds/build_old3ds.sh`. It writes the ELF and
provenance to `out/old3ds/`; site preparation rejects older runtime provenance
without the v1.2 streaming fixes. After building the tools described in the
notices, prepare a fresh output directory:

```sh
python3 browser-builder/scripts/prepare-site.py --out /path/to/empty/site-output
```

The Level 2 world stage applies the two tested road-first file permutations after
visual preparation. Original chunk bodies and accepted visual features remain
unchanged. This reduces a demonstrated ground gap; remaining streaming limits
are documented in [the investigation](../docs/OLD3DS_WORLD_STREAMING.md).

The generated directory contains the runtime, processing tools, corresponding
tool source and static application files. The script rejects completed CIAs and
retail game-data file types in the website. Use an empty output directory so old
test pages and obsolete generated files cannot enter a deployment.

Serve the output over HTTPS, or localhost for development. Production hosting
must serve `.wasm` as `application/wasm`. No application backend, account or API
key is required by this builder.
