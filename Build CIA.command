#!/bin/bash
# Double-click for a local CIA build; no GitHub login or publication.
set -uo pipefail
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:/opt/devkitpro/tools/bin:$PATH"
python3 tools/3ds/make_cia.py "$@"
build_status=$?
if [[ -t 0 ]]; then
    printf '\nPress Return to close this window. '
    read -r
fi
exit "$build_status"
