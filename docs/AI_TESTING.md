# AI-assisted testing

One useful role for AI here was helping turn repetitive checks into regression
tests. Adapting the game to two small screens leaves a lot of details to check:
wide icons, tall icons, long mission objectives, tutorial text and repeated
sleep/wake events. Checking one example by hand does not cover the others.

## Mission icons

Take the mission icons on the lower screen. There are 150 of them, and their
visible artwork does not always fill the image. A wide vehicle and a tall
location marker need to fit the same space without being stretched or clipped.

The checks in [`ui_assets.py`](../tools/3ds/tests/ui_assets.py) compare each
icon's dimensions and visible bounds against the original PNG. They then exercise
the game's fitting code at both HUD and pause-menu sizes, using both filename
forms the game expects. That gives 600 fitting combinations from 150 icons,
two sizes and two name forms, as well as the source-image checks.

This is the kind of repetition AI helped write checks for. The actual comparisons
run in the test harness; a model is not judging each icon or playing the game.

## Text and repeated actions

[`context_text.py`](../tools/3ds/tests/context_text.py) checks contextual strings
with both original fonts. It checks that wrapping keeps every character, stays
within the available width and leaves the last line reachable when scrolling.
[`handheld_text.py`](../tools/3ds/tests/handheld_text.py) also checks tutorial
layout, including the space needed for the button prompts.

Another example is [`lid_audio.cpp`](../tools/3ds/tests/lid_audio.cpp): it checks
1,000 shuffle cycles for the lid greetings, then exercises mute, rapid reopening
and returning from HOME. These are repeated state checks, not 1,000 physical lid
openings.

## Running the interface checks

With Python 3, Clang and Pillow installed, run from the repository root:

```sh
python3 tools/3ds/tests/ui_assets.py
python3 tools/3ds/tests/context_text.py
python3 tools/3ds/tests/handheld_text.py
```

The icon and contextual-text checks need your prepared game assets under
`sdcard/3ds/simpsons/`. The tutorial check can also run synthetic fixtures without
them; its output says which data it used. Retail assets are not included here.

These checks help catch layout and state mistakes. Visual review and testing on
a physical 3DS are still needed; passing them does not prove console performance.
