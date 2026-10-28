# Browser builder validation — 28 October 2026

The builder packages the frozen accepted v1.2 executable from source commit
`61d1c78ad6013e3a4548f1a2014892ff008ee48c`. It does not change game runtime code.
The original ELF SHA-256 is
`9db418bfd7d45f5ec5bd64ea81a14c74dd84f5ea8b7e4158f02782de7b95a5e0`;
the stripped website runtime SHA-256 is
`fa69c71c57bcb022eddf632ec9a047ae89014d6583504068249bbbaaabbff34c`.

The preparation gate verifies disabled game diagnostics and receiver hooks,
accepted early stereo opaque submission, visual mask 255, Title ID
`000400000f3a1100`, version 1.2.0 (1056), native texture profile and the original
3DS memory configuration. Website output contains tools, port runtime, source
archives and recipe metadata. Retail assets, complete CIAs and development test
pages are excluded.

## Desktop ISO-to-CIA proof

A 2,049,269,760-byte English PC image completed all five phases in the Chromium
in-app browser on this Mac. The build finished in **18:42** before the additional
ETC1 block-cache optimization. This is an observed complete run, not a universal
duration promise or a matched browser performance comparison.

| Phase | Seconds |
| --- | ---: |
| Inspect, identify and extract | 84.98 |
| Textures and HOME Menu artwork | 698.91 |
| All v1.2 visual enhancements and DSP cache | 130.56 |
| Voices and 16 complete cinematics | 153.57 |
| CIA packaging | 54.61 |

The private CIA is 1,928,516,544 bytes. Independent read-only inspection found:

- 3,753 RomFS files and 3,525 byte-exact unchanged PC files.
- All 201 required v1.2 enhancements: 193 byte-exact files and eight P3D files
  with equivalent decoded RGBA pixels and identical remaining chunk contents.
  Those eight differences are PNG compression/metadata containers only.
- All 3,206 native texture entries in the exact accepted 17,079,768-byte bundle,
  SHA-256 `23ed6c0ee9eeaa82cd96325c7da8380d5fe3a5634f26ccb6dca1a32590ba1981`.
- Exact accepted executable code, DSP cache and landmark table; ten valid
  character greeting containers; 16 cinematic headers matching their validated
  decoded frame counts and complete audio lengths.
- Correct CIA version and Title ID, with successful `ctrtool --verify` content
  and filesystem checks. Homebrew signature limitations remain unchanged.

The browser reported 187,695,104 bytes of texture Python heap, 324,403,200 bytes
of visual Python heap and 67,108,864 bytes of makerom heap. These are individual
WASM heap observations, not total browser process memory or measured phone peaks.

Safari 27.0.1 on macOS also completed the full ISO-to-CIA path in **18:37**, using
the added exact ETC1 block cache. The normal Save CIA button downloaded the full
1,928,516,544-byte installer. Independent inspection passed the same 201
enhancements, accepted executable, exact native bundle, unchanged PC data,
cinematic and container checks. Its phase times were 119.43 seconds for
inspection/extraction, 656.99 for textures, 138.47 for v1.2 visuals, 166.49 for
media and 34.53 for packaging. Different browser engines prevent treating these
two complete-run durations as a causal optimization comparison.

## Bounded packaging and exact visual preparation

An independent same-input full-size WASM packaging comparison used 3,752
previously prepared files. Both implementations produced identical NCCH content
and preserved every input file. Heap use fell from 1,940,586,496 to 67,108,864
bytes (96.54%); packaging time changed from 35.74 to 43.08 seconds. This proves
the packaging change, independently of the later full source-conversion proof.

Exact duplicate ray-query reuse reduced the seven-map preparation test from
116.55 to 89.15 seconds. All enhancement stages fell from 123.94 to 95.84 seconds
(22.67%), preserving all 200 generated visual files exactly. These measurements
used native Python with actual Node WASM ray calls on this Mac. They are not
full-browser or mobile timings. The 38-clip DSP cache also matches the accepted
output exactly.

The added ETC1 adapter cache uses exact 64-byte RGBA block equality after hashing
and retains the unmodified high-quality encoder on every miss. A clean Node WASM
ABBA comparison of 65 sampled real RGB inputs produced identical encoded output
in all four runs. Mean time fell from 26.89 to 22.35 seconds (**16.89%**), with
module heap size changing from 16,908,288 to 19,529,728 bytes. Both full desktop
builds also reproduced the complete accepted texture bundle. This sample timing
is not a full-build speed claim. An earlier longer sample overlapped the start
of Safari conversion and is retained only as exact-output evidence.

## Safari export and mobile scope

Safari 27.0.1 on macOS created a synthetic OPFS file of 2,040,109,465 bytes,
downloaded it through an OPFS File URL, and reopened the saved file. Exact length
and sampled first, middle and tail sentinels passed. This is sampled export
verification, not a full-file hash or an iPhone test.

Safari's fetch-based local service-worker HEAD and byte-range checks passed,
but its download manager bypassed that worker and received a server 404 page.
Production saving therefore uses an OPFS File URL when a native save picker is
unavailable. No whole-CIA JavaScript array buffer or Web Share copy is required.

The layout was inspected at a 390-pixel phone viewport. Mobile scheduling caps
texture encoding at two workers and releases each major phase before starting
the next. These changes prepare the mobile path; physical iPhone and Android
end-to-end support must still be verified on those devices. Mac Safari and
responsive desktop layout are not substitutes for that evidence.

No newly generated CIA was installed on a physical 3DS during this builder work.
Game performance acceptance remains the earlier matched hardware evidence for
the frozen runtime. Builder elapsed time is not a game FPS measurement.
