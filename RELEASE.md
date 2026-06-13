# Sonic the Hedgehog 2 — macOS (Apple Silicon) build

Native arm64 macOS build of Sonic the Hedgehog 2, attached to release **v0.3.0** as
`SonicTheHedgehog2Recomp-macos-arm64.zip`.

## What this is
- The original game statically recompiled to native arm64 (no emulator core shipped).
- Self-contained `.app`: SDL2 bundled via `@executable_path`, ad-hoc codesigned.
- Verified by manual play on Apple Silicon (looks/sounds correct on the golden path).


## Install
1. Download `SonicTheHedgehog2Recomp-macos-arm64.zip` from the **v0.3.0** release and unzip.
2. First launch: right-click `Sonic the Hedgehog 2.app` -> Open (ad-hoc signed), or
   `xattr -dr com.apple.quarantine "Sonic the Hedgehog 2.app"`.
3. ROM not included — supply your own dump: Sonic the Hedgehog 2 (Genesis) .bin/.md dump
4. Run: `"Sonic the Hedgehog 2.app/Contents/MacOS/Sonic the Hedgehog 2" /path/to/rom`

## Build it yourself
`scripts/release-mac.sh` reproduces this artifact (build -> .app -> zip);
`scripts/release-mac.sh --publish` re-attaches it to the latest release.
Requires: `brew install cmake ninja sdl2 dylibbundler` on Apple Silicon.
