#!/usr/bin/env bash
# Build Sonic 2 with the pinned engine and recomp-ui Switch providers.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$repo/segagenesisrecomp/tools/build-switch.sh" \
    --games-root "$(dirname "$repo")" --sonic2-repo "$repo" \
    --game sonic2 "$@"
