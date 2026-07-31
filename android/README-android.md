# Sonic 2 Recomp — Android build

Sideload-oriented Android port of the Sonic 2 native runner (arm64 only).
The gradle project wraps SDL 2.28.5's `android-project` template; the game
ships as `libmain.so` loaded by `org.libsdl.app.SDLActivity`.

## Layout

- `external/` — fetched, not committed (gitignored):
  - `external/SDL2-2.28.5/` — SDL source (the engine's bundled Windows
    devel package has no source):
    `curl -L https://github.com/libsdl-org/SDL/releases/download/release-2.28.5/SDL2-2.28.5.tar.gz | tar xz -C external/`
  - `external/libucontext/` — ISC-licensed ucontext implementation; bionic
    declares but does not implement makecontext/swapcontext, which
    `runner/fiber_compat.c` needs (aliased in under `__ANDROID__`):
    `git clone https://github.com/kaniini/libucontext external/libucontext`
    (built at `49e671dd52ff`; any nearby commit should do).
- `generated/sonic2/` — host-emitted generated C. The recompiler is a host
  tool and cannot run inside the NDK cross-build, so regenerate by hand after
  ROM/toml/recompiler changes:

  ```sh
  cd ../segagenesisrecomp/sonicthehedgehog2
  ../../build/GenesisRecomp sonic2.bin --game game.toml \
      --output-dir ../../android/generated/sonic2
  ```

  (No `--reverse-debug` — the device build is a stripped native build.)
- `app/jni/SDL` → symlink to the SDL source; `app/jni/shim-include/SDL2` →
  symlink so the runner's `<SDL2/SDL.h>` include style resolves.
- `app/jni/src/CMakeLists.txt` — the real target; mirrors the desktop source
  set minus launcher UI, netplay, cosim, reverse-debug.

## Build + install

```sh
export JAVA_HOME="$HOME/Library/Java/JavaVirtualMachines/jbr-21.0.11/Contents/Home"
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Device files

Everything lives in the app's external files dir (created on first launch):
`/sdcard/Android/data/tech.clyde.sonic2recomp/files/`

```sh
PKG=tech.clyde.sonic2recomp
DST=/sdcard/Android/data/$PKG/files
adb push sonic2.bin $DST/
adb push annotations_from_disasm.csv $DST/        # crash-report symbols
adb push settings.ini $DST/                       # widescreen = 1 lives here
adb push debug.ini $DST/                          # opens TCP cmd server :4380
adb forward tcp:4380 tcp:4380                     # then probes work from the host
```

The runner (under `__ANDROID__`) anchors all config/saves there, `chdir`s to
it, and boots the ROM from `rom-Sonic2.cfg`, else the first
`*.bin`/`*.md`/`*.gen`/`*.smd` it finds there.

The `adb push` is optional: the launcher activity (`RomGateActivity`) is a
ROM gate. If no ROM is in the files dir it shows an in-app picker (Storage
Access Framework — no storage permissions) and copies the chosen file to
`sonic2.bin` there, deleting any stale `rom-*.cfg` so the engine rescans.
The copy is CRC32-verified against Sonic 2 (World, Rev A) `7B905383`; a
mismatch gets a "use anyway?" warning since the recompiled code was
generated from Rev A. With a ROM already present the gate forwards to the
game with no visible UI.

Widescreen is not compiled in as a fork: it is the same build with
`widescreen = 1` in `settings.ini` (or `GENESIS_WIDESCREEN=1` on desktop).
Delete the line to get the authentic 4:3 build.

`settings.ini` must also carry `fullscreen = 1`: SDL's Android backend only
enters immersive mode (status bar hidden, true 1920x1080) when the window is
created fullscreen — with `fullscreen = 0` Android keeps the status bar and
hands the app a 1920x1025 surface.

## Dev warp panel (dual-screen devices)

On devices with a presentation-capable second display (AYN Thor: displayId 4
"Screen-2", 1240x1080 touch), the app shows a developer panel there with
two pages, switched by the 🗺/⚙ buttons inline with the header:

- **Status** (default): live HUD polled every ~300 ms — rings, lives,
  emeralds, score, timer, player/camera hex — plus a real minimap: the
  level layout RAM at `$FF8000` ($1000 bytes; even 128-byte half-rows are
  the FG plane, one byte per 128×128 chunk) is fetched in a single 4 KB
  `read_memory` and drawn as a chunk-occupancy silhouette, overlaid with
  the camera viewport, the player dot, and the boss camera-lock line on
  boss acts. Map extent comes from occupancy, not camera bounds (bounds
  read unclamped 0x3FFF in some acts). Tap the map to toggle between
  fit (whole act, letterboxed) and zoom (fills the view height and
  auto-pans to keep the player centered, clamped at the level edges).

  **Full-art minimap (optional, user-supplied):** if
  `<files dir>/maps/<slug>.png` exists for the current act, the panel
  draws that instead of the silhouette (same overlays — sonicgalaxy.net's
  renders are 1:1 with the level pixel grid, so no calibration needed).
  The images are SEGA level art: never committed or shipped, fetch them
  yourself:

  ```sh
  mkdir -p maps
  for m in ehz-1 ehz-2 cpz-1 cpz-2 arz-1 arz-2 cnz-1 cnz-2 htz-1 htz-2 \
           mcz-1 mcz-2 ooz-1 ooz-2 mz-1 mz-2 mz-3 scz-1 wfz-1 dez-1; do
    curl -sf -o maps/$m.png \
      "https://www.sonicgalaxy.net/wp-content/img/maps/gen/sonic2/$m.png"
  done
  adb push maps /sdcard/Android/data/tech.clyde.sonic2recomp/files/
  ```

  Note the site's naming: Metropolis is `mz` (mz-3 = MTZ act 3) and the
  single-act zones are `scz-1`/`wfz-1`/`dez-1`. HPZ has no retail map —
  it (and any missing file) falls back to the silhouette.
- **Warp**: zone/act warp grid, boss warps, live 16:9⇄4:3 toggle,
  save/load slots 1-4 (files interop with the desktop F-keys and the
  LB/RB gamepad quicksave).
- **Cheats** (✨): +1 life / +rings / 7 emeralds (with the HUD-redraw
  flags at $FFFE1C/1D), Super Sonic (seeds the real transformation —
  flag+palette to 1, the game runs the fade and advances the flag
  itself; tops rings up to 50 since super drains 1/s), a permanent
  invincibility toggle (status_secondary bit 1, timer left 0 so the
  monitor countdown never clears it — INV/SHOES badges show on the
  status page), the authentic 20 s speed shoes burst, and character
  swap (Sonic & Tails / Sonic / Tails — sets Player_option at $FFFF72
  and restarts the act through the warp flow, since player art loads
  at level init).

Java only, in `app/src/main/java/tech/clyde/sonic2recomp/`:
`MainActivity` (SDLActivity subclass, Presentation lifecycle) →
`DevPanelPresentation` (programmatic UI, two pages) + `MinimapView` →
`DebugClient` (one owned socket to the runner's TCP cmd server on
127.0.0.1:4380, HandlerThread-serialized, snapshot poller)
→ `WarpEngine` (Java port of `tools/warp.py` — that script stays the
reference; keep them in sync). Boss spots in `Zones.BOSS_SPOTS` were
extracted from the ROM's LevEvents camera-lock writes and verified live.

The panel needs `debug.ini` on the device (it IS the debug server gate);
without it the panel shows "debug server off" and the game runs normally.
The cmd server accepts ONE client: tap **Release** on the panel before
using host-side probes over `adb forward`, and reconnect after. On
single-screen devices nothing changes — the panel simply never shows.

Two hard-won window details: the panel window is `FLAG_NOT_FOCUSABLE` —
Presentation dialogs are focusable by default, and a focusable window on
the bottom screen steals top-display focus on touch, which deprioritizes
the game's window (frame/audio stutter until the top screen is tapped).
And Presentation cancels ITSELF on display config changes (the widescreen
resize used to trip this), so `MainActivity` re-shows the panel on any
dismissal it didn't initiate.

## Credits

Launcher icon: ["Sonic The Hedgehog 2 Dock Icon"](https://www.deviantart.com/lexiloo826/art/Sonic-The-Hedgehog-2-Dock-Icon-1018993923)
by **LexiLoo826** (DeviantArt), used with attribution under
CC BY-NC-ND 3.0 (resized only, per the artist's "feel free to use, just
please credit me"). The mipmap PNGs in `app/src/main/res/` are generated
from it; `tools/make_icon.py` holds the previous hand-drawn fallback icon.
