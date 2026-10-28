# Browser builder tools and source

The browser builder downloads JavaScript/WebAssembly processing tools. It does
not download a complete game-data package. The user's PC game files are selected
locally and are not part of these tool distributions.

The following notices cover the processing tools, not the original game source
or the user's game files. Existing notices in the port source remain in place.

| Component | Pinned source | License and changes |
| --- | --- | --- |
| makerom 0.18.4 | [3DSGuy/Project_CTR, c0488dcb6c3048e6a519716f2b21e2c65859ca75](https://github.com/3DSGuy/Project_CTR/tree/c0488dcb6c3048e6a519716f2b21e2c65859ca75/makerom) | makerom is MIT licensed. The binary also links GPL-3.0-or-later libblz, Apache-2.0 mbedTLS, and MIT libyaml. The combined binary is distributed with GPL corresponding source and all component notices. Built for WebAssembly; source algorithm unchanged. |
| libblz | Bundled with the pinned Project_CTR source | Copyright (C) 2011 CUE. GPL-3.0-or-later. The full notice is retained in its source. |
| mbedTLS | Bundled with the pinned Project_CTR source | Apache-2.0 unless a particular file states otherwise. |
| libyaml | Bundled with the pinned Project_CTR source | MIT, Copyright (c) 2006 Kirill Simonov. |
| bannertool 1.2.3 | [carstene1ns/3ds-bannertool, 734d33be79fd3f8c29c6296158f06ac7c5ca9dcb](https://github.com/carstene1ns/3ds-bannertool/tree/734d33be79fd3f8c29c6296158f06ac7c5ca9dcb) | MIT. The variable-length padding initializer is changed to a four-byte buffer for Clang portability. Bundled stb_image/stb_vorbis, dr_wav, and utf8cpp retain their own source notices. |
| ETC1 encoder adapter | [devkitPro/tex3ds 2.3.0, 37220c696e873812df41af62d1db0b20c66661fe](https://github.com/devkitPro/tex3ds/tree/37220c696e873812df41af62d1db0b20c66661fe) | The adapter follows tex3ds ETC1 tile and byte ordering and is GPL-3.0-or-later. It uses the unmodified rg_etc1 encoder at high quality. |
| rg_etc1 | Included in the pinned tex3ds source | zlib license, Copyright (c) 2012 Rich Geldreich. |
| Native texture conversion | Current port's `texture.cpp`, `texturetile.hpp`, `swizzle.cpp`, and `decompress.cpp` | Production conversion functions are extracted without algorithm changes. The DXT decompressor retains its MIT notices for Matthäus G. Chajdas and Benjamin Dobell. |
| Emscripten 4.0.23 | [emscripten-core](https://github.com/emscripten-core/emscripten/tree/4.0.23) | MIT / University of Illinois-NCSA and component licenses. Compiler support code notices remain in the generated modules and the upstream source. |
| FFmpeg 5.1.4 | [4729204c17f756e186d622060088371d10b34f7e](https://github.com/FFmpeg/FFmpeg/tree/4729204c17f756e186d622060088371d10b34f7e), [ffmpeg.wasm integration 71aa99d37c02a7b4c435275ca9ef50e612f6efa1](https://github.com/ffmpegwasm/ffmpeg.wasm/tree/71aa99d37c02a7b4c435275ca9ef50e612f6efa1) | LGPL-2.1-or-later FFmpeg code and the MIT ffmpeg.wasm JavaScript wrapper. The smaller build enables only the codecs, containers, and filters required by this builder. It does not link x264, x265, or other external GPL codecs. The complete corresponding source and build recipe are in `ffmpeg-sources.tar.gz`; full notices are in `licenses/`. |
| zlib 1.3.1 | [Exact upstream source](https://github.com/madler/zlib/tree/v1.3.1) | zlib license. FFmpeg PNG support uses Emscripten's pinned zlib port; the complete zlib source is included in `ffmpeg-sources.tar.gz`. |
| FreeArc 0.67 decompression | [mirror/freearc 71f3ab36df26401fff4301b4c8600a31a90d8da9](https://github.com/mirror/freearc/tree/71f3ab36df26401fff4301b4c8600a31a90d8da9), [upstream project](https://sourceforge.net/projects/freearc/) | FreeArc's project metadata specifies GPLv2. Components retain their separate notices: Tornado GPL; GRZip LGPL-2.1-or-later; Igor Pavlov's LZMA/BCJ/AES code public domain; LibTomCrypt's free-use notices and cipher origin acknowledgments. Delta's author expressly permits its decompression code for decompression of FreeArc archives, and that is its only use here; it is not relabeled as GPL-only. Full source and notices are in `freearc-sources.tar.gz` and `licenses/`. |
| Pyodide 0.27.7 | [Exact release source](https://github.com/pyodide/pyodide/tree/0.27.7), [source download](https://github.com/pyodide/pyodide/archive/refs/tags/0.27.7.tar.gz) | MPL-2.0. Unmodified upstream browser runtime. The source link supplies the Source Code Form and build recipes; a copy of MPL-2.0 is in `licenses/`. |
| CPython 3.12.7 | [Exact release source and license](https://github.com/python/cpython/tree/v3.12.7) | Python Software Foundation License Version 2 and historical component notices. CPython is included in the Pyodide runtime and standard library. The full upstream license is in `licenses/`. |
| Pillow 10.2.0 | [Exact release source](https://github.com/python-pillow/Pillow/tree/10.2.0), [Pyodide wheel distribution](https://cdn.jsdelivr.net/pyodide/v0.27.7/full/) | MIT-CMU. The wheel is `pillow-10.2.0-cp312-cp312-pyodide_2024_0_wasm32.whl`, SHA-256 `43eab147a9355385a99eeac18fb577d4af2fe4bd0bbd23d3ba3ddb92e72c2815`. Its full license is in `licenses/`. |
| Pillow image/font dependencies | [Pyodide 0.27.7 Pillow build recipe](https://github.com/pyodide/pyodide/blob/0.27.7/packages/Pillow/meta.yaml), [Emscripten 3.1.58 ports](https://github.com/emscripten-core/emscripten/tree/3.1.58/tools/ports) | The wheel links zlib 1.2.13, libpng 1.6.39, IJG libjpeg 9c, FreeType's `version_1` Emscripten port, libwebp 1.2.2, and libtiff 4.4.0. Their zlib, PNG, IJG, FreeType, BSD and TIFF notices are in `licenses/`. |

Full license texts are provided in `licenses/`. The `wasm-tools-sources.tar.gz`
download contains the pinned corresponding source for these tool builds, local
adapters, the generated native texture translation, and the build command.
The separate `ffmpeg-sources.tar.gz` download contains the complete FFmpeg source,
the ffmpeg.wasm command-line integration and JavaScript wrapper, zlib source,
and the media converter build command. No retail game data is included in either
source archive. The downloadable `tool-manifest.json` and `ffmpeg-manifest.json`
record the precise versions and SHA-256 hashes.

The FFmpeg build includes the XMV demuxer and WMV2 decoder. Its Xbox IMA ADPCM
sample-count correction for WAV tag `0x0069` follows FFmpeg 8's dedicated Xbox
decoder: the last padding sample of each decoded block is excluded. This change
is recorded in the build script and corresponding source; ordinary IMA WAV
decoding is unchanged.

`freearc-sources.tar.gz` and `freearc-manifest.json` cover the separate disc
extraction codec. The local adapter schedules GRZip and 4x4 blocks sequentially
with the original decoding functions. GRZip's pointer-table allocation is made
portable for native 64-bit verification, and source paths are normalized. The
browser reads installer metadata as data to locate the archive keys and checks
the original metadata/file CRCs. Installer executables and scripts are never
executed, and no archive password is embedded in the distributed tool.

To rebuild as a maintainer, install Python 3 and Git, unpack the corresponding
source at a new directory, then run:

```sh
python3 browser-builder/scripts/build-wasm-tools.py --bootstrap
python3 browser-builder/scripts/build-ffmpeg.py --bootstrap
python3 browser-builder/scripts/build-freearc.py --bootstrap
```

Emscripten and the processing tools are prepared under `out/browser-tools`.
Visitors using the browser builder do not run this command or install those
tools. `tool-manifest.json` records source revisions and artifact SHA-256 hashes.

The WebAssembly ETC1 output is checked against native tex3ds 2.3.0, using the
same high-quality encoder, 8×8 tile order, and little-endian ETC1 block bytes.

NumPy 2.0.2 supplies array mathematics for v1.2 scenery preparation. Its BSD and bundled component notices ship in `licenses/numpy-LICENSE.txt`, extracted from the pinned Pyodide wheel. The exact wheel metadata is included in `vendor/pyodide/pyodide-lock.json`.
