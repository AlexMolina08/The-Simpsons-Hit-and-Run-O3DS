# PC copy language support

The builder accepts installed PC folders and supported disc or installer images.
Three-CD releases use split InstallShield cabinets; some images combine those
volumes. Multilingual Inno/FreeArc installers store languages in separate
archives. The builder reads these archives without running a Windows installer.

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
Most game text comes from the selected retail table; a small set of French text
receives spelling corrections. Font coverage and tutorial wrapping tests use the
actual game font where available.
French HUD artwork is converted from its own source, checked against its own
accepted fingerprint; the English HUD is never substituted.

## Validation

Cabinet extraction checks each file against its original MD5. Language detection
requires the selected dialogue archive and movie audio track. Tests also cover
archive bounds, ZIP checksums, path validation, Unicode controls and tutorial
layout. French HUD artwork uses its own verified atlas.

Package integrity, emulator presentation and physical-console gameplay are
separate checks. A successful extraction or ARM build does not establish that
all localized screens and missions work on hardware.
