#!/bin/bash
# Legacy launcher; public uploads of complete game-data CIAs are disabled.
set -uo pipefail
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:/opt/devkitpro/tools/bin:$PATH"
python3 tools/3ds/release_cia.py "$@"
release_status=$?
if [[ -t 0 ]]; then
    printf '\nPress Return to close this window. '
    read -r
fi
exit "$release_status"
