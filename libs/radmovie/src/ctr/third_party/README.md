# Old 3DS movie decoder dependency

The Old 3DS movie backend links the MPEG-1 decoder and parser from FFmpeg 7.1.1,
commit `db69d06eeeab4f46da15030a80d539efb4503ca8`:

https://github.com/FFmpeg/FFmpeg/tree/n7.1.1

`tools/3ds/build_old3ds_ffmpeg.sh` downloads that revision, applies the included
`ffmpeg-newlib.patch`, and builds static ARMv6K libraries without GPL components,
networking, programs, or hardware decoders. FFmpeg is licensed under LGPL 2.1
or later; the license is reproduced in `COPYING.LGPLv2.1`. Preserve this notice
and provide the dependency source, patch and relinkable build when distributing
binaries. The repository build script relinks the executable against locally
modified FFmpeg libraries. Converted game movies are private assets and are
not included in source control.

FFmpeg uses 32-bit enums. Its C++ adapter is compiled with `-fno-short-enums` and
exposes only opaque pointers and fixed-size frame fields to the game. Game and
PDDI objects retain the devkitARM enum ABI. Linker enum-size warnings at this
isolated boundary are expected; FFmpeg enums never cross that interface.
