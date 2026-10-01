#!/usr/bin/env bash
# build-linux.sh — Linux build + AppImage packaging for Sonic the Hedgehog 2.
#
# Linux counterpart to tools/make_release.ps1 (Windows). Same prod-vs-debug
# discipline:
#
#   prod  (default) — strips ALL developer tooling: no TCP cmd server, no
#                     frame_snapshots ring, no chip_trace, no reverse debugger.
#   debug           — compiles the TCP cmd server + observability rings back in.
#
# Configures + builds with cmake, then wraps the ELF into a self-contained
# x86_64 AppImage. State policy (identical to the Windows zip, where everything
# lives next to the exe): settings.ini, rom-Sonic2.cfg, campaign SRAM and
# quickstates all live NEXT TO the .AppImage file. The runner anchors there
# itself — main.c init_exe_dir() prefers $APPIMAGE over /proc/self/exe so state
# never resolves into the read-only squashfs mount. The AppRun:
#   * seeds rom-Sonic2.cfg from a Sonic 2 ROM sitting beside the .AppImage
#     (never passes it as argv[1] — a positional ROM skips the launcher),
#   * saves the host LD_LIBRARY_PATH for recomp-ui's host file pickers,
#   * exports the SDL hints that make a Steam Deck pad read as a real gamepad.
#
# After packaging, tools/test_appimage_layout.sh runs against the AppDir (and
# boots the packaged binary headless against game/sonic2.bin); the build FAILS
# if state lands inside the read-only payload, a user edit does not survive a
# relaunch, or the packaged binary does not boot.
#
# Usage:
#   bash tools/build-linux.sh                  # prod AppImage (default)
#   bash tools/build-linux.sh --config debug   # debug build (TCP server + rings)
#   bash tools/build-linux.sh --no-netplay     # build without rollback netplay
#   bash tools/build-linux.sh --run            # launch the AppImage after building
#   bash tools/build-linux.sh --no-package     # configure + build only, skip AppImage
#   bash tools/build-linux.sh --out DIR        # where to drop the .AppImage
#   bash tools/build-linux.sh --jobs N         # parallel build jobs (default: nproc)
#
# The version is automatic: CMakeLists.txt derives it from git (tag v0.8.2 ->
# 0.8.2; off a tag 0.8.2-3-g<sha>; "-dirty" with uncommitted edits). It is the
# netplay lobby key and names the AppImage — tag the commit to cut a release.
#
# The generated C is produced at build time from game/sonic2.bin (the ROM is a
# build dependency), so the owner ROM must be present. It is NEVER packaged.
#
# Prereqs: cmake, a C/C++ toolchain, libsdl2-dev, libgl1-mesa-dev, git, curl,
# file. ImageMagick (optional) turns boxart.png into the AppImage icon.
# linuxdeploy/appimagetool are fetched into the build tree and verified against
# pinned SHA-256s (reproducible packaging).
set -euo pipefail

# ============================ PER-GAME CONFIG ===============================
APP_NAME="Sonic the Hedgehog 2"
RELEASE_SLUG="SonicTheHedgehog2Recomp"      # matches the windows zip prefix
CMAKE_TARGET="SonicTheHedgehog2Recomp"
SHORT_NAME="Sonic2"                         # g_game_spec.short_name -> rom-<short>.cfg
ROM_NAME="sonic2.bin"                       # preferred adjacent ROM name
ROM_EXTS="bin md gen smd"
BUILD_ROM="game/sonic2.bin"                 # generation input (never shipped)
BOXART="boxart.png"
PROD_CMAKE_FLAGS=( -DGEN_ENABLE_TRACE=OFF -DGEN_DEV_TRACE=OFF -DSONIC_REVERSE_DEBUG=OFF )
DEBUG_CMAKE_FLAGS=( -DGEN_ENABLE_TRACE=ON -DGEN_DEV_TRACE=ON -DSONIC_REVERSE_DEBUG=ON )
# ============================================================================

# Versioned assets and digests from the upstream GitHub release metadata.
# Floating continuous URLs can replace their payload and invalidate a pin.
LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
LINUXDEPLOY_SHA=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
APPIMAGETOOL_URL=https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage
APPIMAGETOOL_SHA=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0

CONFIG="prod"
DO_RUN=0
DO_PACKAGE=1
NETPLAY=ON
JOBS="$(nproc 2>/dev/null || echo 4)"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/release-linux"

while [ $# -gt 0 ]; do
  case "$1" in
    --config) CONFIG="$2"; shift 2;;
    --prod) CONFIG="prod"; shift;;
    --debug) CONFIG="debug"; shift;;
    --no-netplay) NETPLAY=OFF; shift;;
    --run) DO_RUN=1; shift;;
    --no-package) DO_PACKAGE=0; shift;;
    --out) OUT="$2"; shift 2;;
    --jobs) JOBS="$2"; shift 2;;
    -h|--help) sed -n '2,48p' "$0"; exit 0;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done
case "$CONFIG" in prod) FLAGS=( "${PROD_CMAKE_FLAGS[@]}" );; debug) FLAGS=( "${DEBUG_CMAKE_FLAGS[@]}" );;
  *) echo "--config must be prod or debug (got '$CONFIG')" >&2; exit 2;; esac

cd "$REPO"

# Engine root resolves exactly as CMakeLists.txt does.
if [ -n "${GENESIS_RECOMP_ROOT:-}" ]; then ENGINE="$GENESIS_RECOMP_ROOT"; FLAGS+=( -DGENESIS_RECOMP_ROOT="$ENGINE" )
elif [ -e "$REPO/engine-local" ]; then ENGINE="$REPO/engine-local"
else ENGINE="$REPO/segagenesisrecomp"; fi

need() { [ -e "$1" ] || { echo "ERROR: $2" >&2; exit 1; }; }
need "$ENGINE/runner/main.c" "engine not initialized at $ENGINE; run 'git submodule update --init --recursive'."
need "$REPO/recomp-ui/recomp_ui.cmake" "recomp-ui is not initialized; run 'git submodule update --init --recursive'."
if [ "$NETPLAY" = ON ]; then
  need "$ENGINE/external/recomp-net/CMakeLists.txt" "netplay needs $ENGINE/external/recomp-net (git submodule update --init --recursive), or pass --no-netplay."
  need "$ENGINE/external/rbengine/CMakeLists.txt" "netplay needs $ENGINE/external/rbengine (git submodule update --init --recursive), or pass --no-netplay."
fi
need "$REPO/$BUILD_ROM" "owner ROM missing: place Sonic the Hedgehog 2 (World) (Rev A) at $BUILD_ROM (generation input; never packaged)."
need "$ENGINE/LICENSE.md" "engine LICENSE.md missing at $ENGINE"
need "$ENGINE/THIRD-PARTY-LICENSES.md" "engine THIRD-PARTY-LICENSES.md missing at $ENGINE"

FLAGS+=( -DGENESISRECOMP_NETPLAY="$NETPLAY" -DBUILD_TESTING=OFF )

# Point cmake at the HOST's Linux SDL2 (the Windows build pins a bundled VC
# devel pack; -DSDL2_DIR is consulted before CMAKE_PREFIX_PATH).
SDL2_CFG_DIR="$( { find /usr/lib /usr/lib64 /usr/local/lib -type d -path '*cmake/SDL2' 2>/dev/null || true; } | head -1 )"
[ -n "$SDL2_CFG_DIR" ] && FLAGS+=( -DSDL2_DIR="$SDL2_CFG_DIR" )

WORK=""
cleanup() { [ -n "$WORK" ] && rm -rf "$WORK"; return 0; }
trap cleanup EXIT

BUILD="$REPO/build-linux-$CONFIG"
[ "$NETPLAY" = OFF ] && BUILD="$BUILD-nonet"
echo "==================== $APP_NAME ($CONFIG, netplay $NETPLAY) ===================="

echo "[1/4] configure ($CONFIG: ${FLAGS[*]})"
cmake -S "$REPO" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "${FLAGS[@]}"
VERSION="$(head -n1 "$BUILD/sonic2_version.txt")"   # derived from git by CMakeLists.txt
echo "      version: $VERSION"
echo "[2/4] build ($CMAKE_TARGET, -j$JOBS)"
cmake --build "$BUILD" --target "$CMAKE_TARGET" -j"$JOBS"

# Locate the produced ELF by magic (the NTFS mount marks every file executable,
# so the exec bit is meaningless here).
BIN=""
while IFS= read -r f; do
  if [ "$(basename "$f")" = "$CMAKE_TARGET" ] && file -b "$f" 2>/dev/null | grep -q "ELF.*executable"; then BIN="$f"; break; fi
done < <(find "$BUILD" -maxdepth 3 -type f)
[ -n "$BIN" ] || { echo "ERROR: no ELF named '$CMAKE_TARGET' under $BUILD" >&2; exit 1; }
echo "      ELF: $BIN ($(du -h "$BIN" | cut -f1))"
# A netplay binary must carry the version it is named as (the lobby key).
[ "$NETPLAY" = OFF ] || grep -aqF "$VERSION" "$BIN" || { echo "ERROR: $BIN is not stamped with version '$VERSION'" >&2; exit 1; }

if [ "$DO_PACKAGE" = "0" ]; then echo "      (--no-package) done."; exit 0; fi

echo "[3/4] package AppImage"
mkdir -p "$OUT"
EXE="$(basename "$BIN")"
SLUG="$(echo "$RELEASE_SLUG" | tr '[:upper:] ' '[:lower:]-' | tr -cd 'a-z0-9-')"
WORK="$(mktemp -d)"   # cleaned by the EXIT trap registered above
APPDIR="$WORK/AppDir"; mkdir -p "$APPDIR"

# Pinned tooling, fetched into the build tree and SHA-verified so packaging is
# reproducible regardless of what happens to live in ~/recomp-tools.
TOOLS_DIR="$BUILD/appimage-tools"
mkdir -p "$TOOLS_DIR"
fetch_tool() { # url sha dest
  local url="$1" sha="$2" dest="$3"
  if [ ! -f "$dest" ] || [ "$(sha256sum "$dest" | awk '{print $1}')" != "$sha" ]; then
    echo "      fetching $(basename "$dest")"
    curl -fL --retry 3 "$url" -o "$dest.tmp"
    printf '%s  %s\n' "$sha" "$dest.tmp" | sha256sum -c - >/dev/null
    mv "$dest.tmp" "$dest"
  fi
  chmod 0755 "$dest"
}
LINUXDEPLOY_BIN="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL_BIN="$TOOLS_DIR/appimagetool-x86_64.AppImage"
fetch_tool "$LINUXDEPLOY_URL" "$LINUXDEPLOY_SHA" "$LINUXDEPLOY_BIN"
fetch_tool "$APPIMAGETOOL_URL" "$APPIMAGETOOL_SHA" "$APPIMAGETOOL_BIN"
LINUXDEPLOY="$LINUXDEPLOY_BIN --appimage-extract-and-run"
APPIMAGETOOL="$APPIMAGETOOL_BIN --appimage-extract-and-run"

# Icon: boxart padded to a square 256x256 when ImageMagick is available, flat
# placeholder otherwise (linuxdeploy only accepts standard square sizes).
ICON="$WORK/$SLUG.png"
IMAGE_TOOL=""
command -v magick >/dev/null 2>&1 && IMAGE_TOOL=magick
[ -z "$IMAGE_TOOL" ] && command -v convert >/dev/null 2>&1 && IMAGE_TOOL=convert
if [ -n "$IMAGE_TOOL" ] && [ -f "$REPO/$BOXART" ]; then
  "$IMAGE_TOOL" "$REPO/$BOXART" -resize 240x240 -background transparent \
      -gravity center -extent 256x256 "$ICON"
else
  python3 - "$ICON" "$SLUG" <<'PY'
import sys, zlib, struct, hashlib
out, slug = sys.argv[1], sys.argv[2]
h = hashlib.md5(slug.encode()).digest()
r, g, b = h[0] | 0x30, h[1] | 0x30, h[2] | 0x30
N = 256; row = bytes([0]) + bytes([r, g, b]) * N; raw = row * N
def chunk(t, d):
    c = t + d
    return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
open(out, "wb").write(png)
PY
fi

cat > "$WORK/$SLUG.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Exec=$EXE
Icon=$SLUG
Categories=Game;
Terminal=false
EOF

$LINUXDEPLOY --appdir "$APPDIR" --executable "$BIN" \
    --desktop-file "$WORK/$SLUG.desktop" --icon-file "$ICON"

# The ImGui pre-boot launcher loads fonts + images from assets/ next to the exe
# (SDL_GetBasePath resolves to usr/bin inside the AppImage). recomp_ui.cmake's
# POST_BUILD staged them beside the build ELF; carry them into the AppDir.
[ -d "$(dirname "$BIN")/assets" ] || { echo "ERROR: recomp-ui launcher assets/ missing beside $BIN" >&2; exit 1; }
echo "      staging launcher assets/ -> AppDir/usr/bin/assets"
cp -r "$(dirname "$BIN")/assets" "$APPDIR/usr/bin/assets"

# License + attribution (RELEASING.md checklist items 2-3), same files the
# Windows zip carries.
DOC="$APPDIR/usr/share/doc/$SLUG"; mkdir -p "$DOC"
cp "$ENGINE/LICENSE.md" "$DOC/LICENSE"
cp "$ENGINE/THIRD-PARTY-LICENSES.md" "$DOC/THIRD-PARTY-LICENSES.md"
cp "$REPO/release/README.txt" "$DOC/README.txt"

# Custom AppRun. State policy: everything user-visible lives NEXT TO the
# .AppImage, exactly like the Windows zip keeps it next to the exe. The runner
# anchors there itself ($APPIMAGE is preferred over /proc/self/exe in
# init_exe_dir), so this script must keep APPIMAGE exported.
rm -f "$APPDIR/AppRun"   # linuxdeploy leaves it a symlink to the real exe
cat > "$APPDIR/AppRun" <<EOF
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\$0")")"
# recomp-ui restores this for host tools (zenity/kdialog) it spawns, so they do
# not load the bundle's libraries against the host GTK/Qt.
export RECOMP_HOST_LD_LIBRARY_PATH="\${LD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="\$HERE/usr/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
# Steam Deck: read the built-in pad as a real gamepad instead of letting Steam's
# desktop layout retype it as keyboard (which otherwise sends Esc on B, etc.).
export SDL_JOYSTICK_HIDAPI_STEAM=1
export SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1
SELF="\${APPIMAGE:-\$0}"
ROMDIR="\$(dirname "\$(readlink -f "\$SELF")")"
# The launcher's Browse dialog starts beside the .AppImage.
export RECOMP_APPIMAGE_PATH="\$(readlink -f "\$SELF")"
is_genesis_rom() { # SEGA at 0x100; interleaved .smd carries no plain header
    case "\$1" in *.smd) return 0;; esac
    [ "\$(dd if="\$1" bs=1 skip=256 count=4 2>/dev/null)" = "SEGA" ]
}
ROM=""
[ -f "\$ROMDIR/$ROM_NAME" ] && ROM="\$ROMDIR/$ROM_NAME"
if [ -z "\$ROM" ]; then
    for ext in $ROM_EXTS; do
        for f in "\$ROMDIR"/*."\$ext"; do
            [ -f "\$f" ] && is_genesis_rom "\$f" && ROM="\$f" && break 2
        done
    done
fi
cd "\$ROMDIR" 2>/dev/null || true
# Seed rom-$SHORT_NAME.cfg from a ROM beside the .AppImage instead of passing it
# as argv[1]: a positional ROM SKIPS the GUI launcher (no route to Settings,
# Mods or Online). The launcher opens with the ROM resolved; "Skip launcher on
# boot" then boots straight off the same path. Never repoint a resolving cache.
if [ -n "\$ROM" ]; then
    CFG="\$ROMDIR/rom-$SHORT_NAME.cfg"
    cached=""
    [ -f "\$CFG" ] && cached="\$(head -n1 "\$CFG" 2>/dev/null | tr -d '\\r\\n')"
    if [ -z "\$cached" ] || [ ! -f "\$cached" ]; then
        [ -w "\$ROMDIR" ] && printf '%s\\n' "\$ROM" > "\$CFG" 2>/dev/null || true
    fi
fi
exec "\$HERE/usr/bin/$EXE" "\$@"
EOF
chmod +x "$APPDIR/AppRun"

# Compliance (RELEASING.md item 4): no ROM, dump, save or build junk in the
# payload, same screen as segagenesisrecomp/tools/package_release.py.
LEAK="$(find "$APPDIR" -type f \( -iname '*.bin' -o -iname '*.gen' -o -iname '*.smd' \
  -o -iname '*.srm' -o -iname 'ramdump*' -o -iname 'savestate*' -o -iname '*.log' \
  -o -iname '*.map' -o -iname '*.md5' -o -iname '*.sha256' -o -iname '*.toml' \) )"
[ -z "$LEAK" ] || { echo "ERROR: forbidden files in AppDir payload:" >&2; echo "$LEAK" >&2; exit 1; }
case "$EXE" in *_cosim*|GenesisRecomp) echo "ERROR: refusing to package dev-only target $EXE" >&2; exit 1;; esac

APP="$OUT/$RELEASE_SLUG-linux-$VERSION-x86_64.AppImage"
rm -f "$APP"
ARCH=x86_64 $APPIMAGETOOL "$APPDIR" "$APP"
chmod +x "$APP"
echo "      BUILT: $APP ($(du -h "$APP" | cut -f1))"

echo "[4/4] layout + boot test (state beside the .AppImage, payload read-only)"
bash "$REPO/tools/test_appimage_layout.sh" "$APPDIR" "$REPO/$BUILD_ROM"

( cd "$OUT" && sha256sum "$(basename "$APP")" > "$(basename "$APP").sha256" && cat "$(basename "$APP").sha256" )

if [ "$DO_RUN" = "1" ]; then echo "[run] $APP"; "$APP" || true; fi
