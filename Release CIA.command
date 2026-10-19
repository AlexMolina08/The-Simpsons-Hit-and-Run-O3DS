#!/bin/bash
# Double-click to build a versioned CIA and publish a GitHub test release.
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
