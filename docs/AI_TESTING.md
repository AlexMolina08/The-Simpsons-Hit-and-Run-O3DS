# AI-assisted testing

AI helped with the testing work: suggesting edge cases and helping write small
host-side regression checks. This made it practical to cover hundreds of input
combinations, including cases that would be awkward to reproduce by playing.
The checks live in `tools/3ds/tests/`, so their inputs and assertions can be read
and rerun.

## Example: changing a mesh's indices

A cached index check must stop being valid when the indices change. Otherwise,
a mesh that was valid earlier could keep using an out-of-range vertex index.
Vertex updates alone should not discard that cached result when the index cache
is enabled.

`index_validation.py` exercises the actual PICA index-buffer methods in a small
host harness. It starts with six indices for four vertices, then changes an index
to `4`, which is outside the valid `0`–`3` range. The check must reject it. It also
tries `65535`, replaces the indices with valid ones, changes the active count,
and checks upload and prepared-draw invalidation.

The same test runs a 10,000-iteration loop of vertex updates and index validation
in each of four configurations: reference, cached indices, deferred upload and
prepared draws. Those are repeated regression checks, not 40,000 different
scenes or gameplay sessions.

From the repository root, with Python 3 and Clang installed:

```sh
python3 tools/3ds/tests/index_validation.py
```

The script compiles each harness with AddressSanitizer and UndefinedBehaviorSanitizer.
A successful run prints a pass line for each configuration and exits with code 0.

These checks can catch mistakes in the tested logic. They do not reproduce the
3DS GPU, prove a frame-rate improvement, or replace playing on a physical console.
