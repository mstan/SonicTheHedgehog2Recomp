#!/usr/bin/env bash
# Product settings for the shared Genesis macOS builder.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${GENESIS_RECOMP_ROOT:-$REPO/segagenesisrecomp}"
if [ -z "${GENESIS_RECOMP_ROOT:-}" ] && [ -e "$REPO/engine-local" ]; then ENGINE="$REPO/engine-local"; fi
APP_NAME="Sonic2"
CMAKE_TARGET="SonicTheHedgehog2Recomp"
ROM_EXTS="md bin gen"
EXTRA_ARGS=""
REGEN_CMD=""
PREBUILD_CMD=""
POSTBUILD_CMD=""
PROD_CMAKE_FLAGS=( -DGEN_ENABLE_TRACE=OFF -DGEN_DEV_TRACE=OFF -DSONIC_REVERSE_DEBUG=OFF )
DEBUG_CMAKE_FLAGS=( -DGEN_ENABLE_TRACE=ON -DGEN_DEV_TRACE=ON -DSONIC_REVERSE_DEBUG=OFF )
BUNDLE_ID="com.mstan.sonic2recomp"
source "$ENGINE/tools/release/build-macos.sh" "$@"
