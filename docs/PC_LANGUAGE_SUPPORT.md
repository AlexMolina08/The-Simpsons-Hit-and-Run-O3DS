# PC copy language support

The original PC release used three CDs, with regional editions and later
BestSeller releases. The supplied Spanish image combines InstallShield cabinet
volumes into one ISO. A filename containing “DVD” does not establish an original
retail DVD release. The supplied multilingual image is an Inno/FreeArc installer
with separate language archives. The builder reads data archives; it never runs
the Windows installer.

Distribution references: [disc catalogue](https://redump.info/discs?dumper=XTac&sort=version),
[PC manual](https://vignette3.wikia.nocookie.net/simpsons-hit-and-run/images/3/35/Hit_and_Run_Manual_PC.pdf).
Installer reader: [libunshield](https://github.com/twogood/unshield), pinned to
1.6.2. Its split, uncompressed CAB reader receives a bounded volume-length fix;
every extracted file still passes its original MD5 check.

| Copy language | Dialogue archive | Text table | Multi-track movie audio |
| --- | --- | --- | --- |
| English | dialog.rcf | E | 0 |
| French | dialogf.rcf | F | 1 |
| Spanish | dialogs.rcf | S | 3 |

An installed copy exposes languages through its actual dialogue archives. The
multilingual image exposes its separate language archives. A single-language
copy is automatic; a multilingual copy requires a selection from its included
languages. The choice participates in checkpoint identity, so one language's
converted sound cannot be reused for another. Logos with one audio stream keep
stream zero. A missing selected track in a multilingual movie fails validation.

The runtime reads `game-language.txt` from the packaged assets before loading
one text table. The PAL build includes the localized dialogue script indexes.
Console locale does not override the selected PC copy. Existing packages without
this file continue in English. Save serialization and title identity do not
change.

Handheld controls and port-specific captions use UTF-8 to UTF-16 conversion.
Mission names and narrative stay in the original selected text table. Font
coverage and tutorial wrapping tests use the actual game font where available.
French HUD artwork is converted from its own source, checked against its own
accepted fingerprint; the English HUD is never substituted.

## Verification in progress

The supplied French installed folder and Spanish InstallShield image were
inspected. Spanish cabinet files spanning volumes now extract with matching
native and WebAssembly hashes. The shared world, character and vehicle sources
match the English reference. The French HUD requires a distinct verified atlas.

Unit tests cover detection, movie track selection, missing dialogue resources,
archive bounds, ZIP checksums and path validation, Unicode controls and tutorial
layout. The ARM runtime builds within the fixed 64 MiB startup budget. Full
browser packaging, emulator presentation and physical-console checks are tracked
separately; this document will be updated with completed evidence.
