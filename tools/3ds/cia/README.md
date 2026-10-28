# HOME Menu assets

Packaging generates the 256×128 banner and 48×48 icon from the user's retail PC
files. `prepare_home_artwork.py` reads `gamelogo.png` from `frontend.p3d` and
`logo.bmp` from `resource/pure3d/logo_anim.p3d`, preserves their proportions, and
writes private PNGs next to the local CIA. Bannertool produces both HOME Menu
icon sizes from that generated source. No game-logo PNG is needed in this folder.

`artwork-source.json` records source and output hashes for reproducibility.

Packaging generates a 2.8-second, non-looping cue from
`sound\music\simpsons_theme.rsd` in the user's `music02.rcf`. It uses 16-bit stereo
PCM at 32 kHz, short fades and a conservative loudness/peak target. No retail
music excerpt is committed to the source repository.

These are homebrew-tool compatibility choices. Nintendo's licensed submission
requirements are not reproduced here and this project does not claim certification.
