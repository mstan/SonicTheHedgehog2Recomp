#!/bin/sh
# test_appimage_layout.sh <AppDir> [<rom>] — prove the packaged layout cannot
# trap user state inside the AppImage, and that the packaged binary boots.
#
# State policy under test (same as the Windows zip, where everything lives
# next to the exe): settings.ini, rom-Sonic2.cfg, campaign SRAM and
# quickstates live NEXT TO the .AppImage file. The runner anchors there because
# init_exe_dir() prefers $APPIMAGE over /proc/self/exe. This script simulates
# the AppImage runtime against a read-only AppDir and asserts:
#   1. a ROM beside the (simulated) .AppImage seeds rom-Sonic2.cfg, and a
#      cache that already resolves is never repointed;
#   2. non-ROM .bin files (saves, dumps) beside it are not mistaken for a ROM;
#   3. with a ROM, the packaged binary boots headless to RUN_DONE using only
#      the bundled libraries, and a user settings.ini survives the run;
#   4. nothing is ever written inside the read-only AppDir payload.
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 /path/to/AppDir [/path/to/sonic2.bin]" >&2
    exit 2
fi

appdir=$(CDPATH= cd -- "$1" && pwd)
rom=${2:-}
exe=SonicTheHedgehog2Recomp
cfg=rom-Sonic2.cfg
[ -f "$appdir/usr/bin/$exe" ] || { echo "no $exe ELF under $appdir/usr/bin" >&2; exit 1; }
[ -x "$appdir/AppRun" ] || { echo "no executable AppRun in $appdir" >&2; exit 1; }
[ -f "$appdir/usr/bin/assets/fonts/LatoLatin-Regular.ttf" ] || {
    echo "FAIL: launcher assets/ missing from payload" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'chmod -R u+w "$appdir" "$tmp" 2>/dev/null || true; rm -rf "$tmp"' EXIT HUP INT TERM

# Launch helper: simulate the AppImage runtime (APPIMAGE path), headless SDL,
# no GUI launcher, no display (a ROM-less automation run would otherwise fall
# through to the host zenity/kdialog picker and block). Discovery-only runs
# pass a nonexistent positional ROM: the AppRun's ROM discovery happens before
# exec regardless, and the binary then exits at load.
run_apprun() { # simulated_appimage_path [args...]
    sim=$1; shift
    mkdir -p "$(dirname "$sim")"
    ( cd "$(dirname "$sim")" && \
      unset DISPLAY WAYLAND_DISPLAY && \
      APPIMAGE=$sim \
      SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      GENESIS_NO_LAUNCHER=1 GENESIS_RUN_DONE=1 \
      timeout 120 "$appdir/AppRun" "$@" ) || true
}

chmod -R a-w "$appdir"

# 1. Adjacent ROM seeds the cache.
s1=$tmp/state1
mkdir -p "$s1"
printf 'not a rom\n' > "$s1/native_save_1.bin"     # 2. must be ignored
if [ -n "$rom" ]; then cp "$rom" "$s1/MyDump.bin"; romname=MyDump.bin
else
    # Minimal header-only stand-in: "SEGA" at 0x100.
    dd if=/dev/zero of="$s1/MyDump.bin" bs=1024 count=1 2>/dev/null
    printf 'SEGA' | dd of="$s1/MyDump.bin" bs=1 seek=256 conv=notrunc 2>/dev/null
    romname=MyDump.bin
fi
run_apprun "$s1/Sonic2.AppImage" "$tmp/absent.bin" >/dev/null 2>&1
test -f "$s1/$cfg" || { echo "FAIL: adjacent ROM did not seed $cfg" >&2; exit 1; }
test "$(head -n1 "$s1/$cfg")" = "$s1/$romname" || {
    echo "FAIL: $cfg does not point at the adjacent ROM: $(cat "$s1/$cfg")" >&2; exit 1; }

# The preferred name wins over other ROM-shaped files, but a resolving cache
# is never repointed (the runner owns rom-Sonic2.cfg once it exists).
cp "$s1/$romname" "$s1/sonic2.bin"
run_apprun "$s1/Sonic2.AppImage" "$tmp/absent.bin" >/dev/null 2>&1
test "$(head -n1 "$s1/$cfg")" = "$s1/$romname" || {
    echo "FAIL: AppRun repointed a $cfg that already resolved: $(head -n1 "$s1/$cfg")" >&2; exit 1; }
rm -f "$s1/$cfg"
run_apprun "$s1/Sonic2.AppImage" "$tmp/absent.bin" >/dev/null 2>&1
test "$(head -n1 "$s1/$cfg")" = "$s1/sonic2.bin" || {
    echo "FAIL: preferred sonic2.bin not chosen: $(head -n1 "$s1/$cfg")" >&2; exit 1; }

# 2b. A directory holding only non-ROM .bin files seeds nothing.
s2=$tmp/state2
mkdir -p "$s2"
printf 'not a rom\n' > "$s2/ramdump_native.bin"
run_apprun "$s2/Sonic2.AppImage" "$tmp/absent.bin" >/dev/null 2>&1
test ! -f "$s2/$cfg" || { echo "FAIL: a non-ROM .bin seeded $cfg" >&2; exit 1; }

# 3. Boot the packaged binary headless against the real ROM.
if [ -n "$rom" ]; then
    s3=$tmp/state3
    mkdir -p "$s3"
    cp "$rom" "$s3/sonic2.bin"
    printf '# user-owned marker\n' > "$s3/settings.ini"
    before=$(cat "$s3/settings.ini")
    # The runner needs an accelerated SDL renderer, which SDL's dummy video
    # driver cannot provide: boot on the session display, else under Xvfb.
    if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then wrap=""
    elif command -v xvfb-run >/dev/null 2>&1; then wrap="xvfb-run -a"
    else echo "FAIL: boot test needs a display or xvfb-run (apt install xvfb)" >&2; exit 1; fi
    out=$( ( cd "$s3" && APPIMAGE=$s3/Sonic2.AppImage SDL_AUDIODRIVER=dummy              GENESIS_NO_LAUNCHER=1 GENESIS_RUN_DONE=1              timeout 300 $wrap "$appdir/AppRun" "$s3/sonic2.bin" --turbo --max-frames 600 ) 2>&1 || true)
    printf '%s\n' "$out" | grep -q "^RUN_DONE frames=600 " || {
        echo "FAIL: packaged binary did not boot to RUN_DONE. Output tail:" >&2
        printf '%s\n' "$out" | tail -n 30 >&2; exit 1; }
    head -n1 "$s3/settings.ini" | grep -qF "$before" || {
        echo "FAIL: user settings.ini clobbered by a run" >&2; exit 1; }
    echo "      boot: RUN_DONE after 600 frames from the packaged payload"
else
    echo "      boot: SKIPPED (no ROM supplied)"
fi

# 4. The read-only payload stayed pristine: no state files anywhere in AppDir.
for leak in settings.ini "$cfg" '*.srm' '*.sav' '*.log' '*.toml' 'ramdump*' 'native_save_*' 'genesis-netplay-room.txt'; do
    found=$(find "$appdir" -name "$leak" | grep -v '^$' || true)
    [ -z "$found" ] || { echo "FAIL: state leaked into the payload: $found" >&2; exit 1; }
done

echo "AppImage layout test passed: ROM discovery seeds $cfg beside the .AppImage, resolving caches are kept, payload stays read-only"
