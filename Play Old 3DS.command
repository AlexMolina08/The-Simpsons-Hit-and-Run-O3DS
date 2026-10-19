#!/bin/bash
set -euo pipefail
emulator_root="$HOME/Applications/hitrun-old3ds"
open "$emulator_root/Azahar.app" --args "$emulator_root/SimpsonsNTSC.3dsx"
