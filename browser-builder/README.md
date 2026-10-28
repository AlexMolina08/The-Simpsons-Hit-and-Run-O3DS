# Browser CIA builder

This static application prepares a 3DS installer from PC game files selected on
the visitor's computer. Conversion and packaging run in Web Workers. Game data
is never included in the website or sent to a server.

The application uses OPFS for temporary output and the File System Access API
to save the completed installer without copying the entire file into JavaScript
memory. Desktop Chrome and Edge are the supported browsers.

## Source layout

- `app.mjs` handles file selection, progress, cancellation and saving.
- `worker.mjs` runs extraction and conversion, then packages in a fresh worker
  after the conversion heaps have been released.
- `engine-conversion/` contains ISO reading and game-data preparation.
- `tools.mjs` adapts the WebAssembly tools and the texture worker pool.
- `opfs.mjs` supplies temporary storage and disk-backed makerom output.
- `scripts/` builds the pinned tools and prepares the static website.
- `THIRD_PARTY_NOTICES.md` and `licenses/` contain tool licensing information.

The builder is an unpublished prototype, tested with an English PC copy. Its
current preparation defaults reference the older v1.1.1 runtime and manifest.
The accepted v1.2 runtime, build settings and prepared visual assets still need
to be integrated and checked before publication. Other languages are unvalidated.

Preparing the website verifies the supplied runtime against its release
manifest, strips debug information into a new file, and records hashes for the
runtime, packaging configuration and texture profile. The original executable
is preserved.

## Maintainer checks

These checks use synthetic fixtures and do not require game files:

```sh
node browser-builder/tests/conversion-unit.mjs
node browser-builder/tests/iso-unit.mjs
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

`tests/production-package.mjs` is an opt-in comparison of full-size native and
WebAssembly packages. It verifies container integrity and streams every RomFS
asset against the private input hashes. It can consume several gigabytes of
temporary disk space; run it only when packaging changes require it.

The HTML tests under `tests/` are local development harnesses. They must not be
copied into a production website. Container, conversion and emulator checks are
separate from installation and gameplay on a physical console.

## Static output

After building the tools described in the notices and providing the accepted
local ELF and release manifest, prepare a fresh output directory:

```sh
python3 browser-builder/scripts/prepare-site.py --out /path/to/empty/site-output
```

The generated directory contains the runtime, processing tools, corresponding
tool source and static application files. The script rejects completed CIAs and
retail game-data file types in the website. Use an empty output directory so old
test pages and obsolete generated files cannot enter a deployment.

Serve the output over HTTPS, or localhost for development. Production hosting
must serve `.wasm` as `application/wasm`. No application backend, account or API
key is required by this builder.
