# Pre-boot netplay launch regression

The v0.8.0-rc1 Windows playtest could close both peers when the online host
started the match. Genesis published its session configuration before main
initialized it, so Sonic 2 rejected an empty roster with exit code 1. The old
headless lobby test initialized the machine and configuration first; LAN also
did not carry these online match capabilities. Both paths missed the failure.

The engine now captures local preferences when the lobby publishes CREATE and
START. Launcher settings and game options can change between those calls.
Host settings remain session-only, separate from the guest's preferences.
`GENESIS_LOBBY_SELFTEST_PREBOOT=1` exercises these same callbacks before machine
initialization, without driving graphical widgets.

Run the release ZIP against an isolated recomp-net-server with its UDP input
relay enabled. Supply your own ROM; it is copied into neither the ZIP nor the
repository. Use a dedicated `--runtime-dir` for test installations and reuse
that same directory on every run. Settings live beside each seat's executable;
new logs and captures go under `--out`. On Windows, changing executable paths
can trigger a fresh Firewall prompt per seat. Run one session at a time with
two peers by default. A runtime lock prevents overlapping runs in that directory.

```powershell
python tools/validate_netplay_launch.py --package SonicTheHedgehog2Recomp-v0.8.1-win64.zip --rom C:/ROMs/sonic2.bin --runtime-dir validation/runtime --out validation/rematches --lobby-url ws://127.0.0.1:18765 --rounds 3 --latency 40 --mispredict 90
python tools/validate_netplay_launch.py --package SonicTheHedgehog2Recomp-v0.8.1-win64.zip --rom C:/ROMs/sonic2.bin --runtime-dir validation/runtime --out validation/online-options --lobby-url ws://127.0.0.1:18765 --frames 3000 --amy C:/ROMs/amy-1.7.1.bin --s3k C:/ROMs/sonic3k.bin --options --latency 40
```

`--exe` optionally replaces the ZIP's executable for development. The script
requires clean exits, all requested rounds, matching confirmed state digests,
no desync/refusal, real rollback episodes when requested, and unchanged local
settings files. Gameplay runs also capture the level and verify its RAM mode.
Results and peer logs remain under `--out`; a failure returns a nonzero status.

`--amy` and `--s3k` enable the corresponding verified donor features in each
isolated seat, with the campaign save menu disabled. `--options` starts with
Sonic/Tails, enters the real online Options screen, changes PLAYERS to 4 and
selects Knuckles/Amy for P3/P4. It captures the menu and requires four actors
in gameplay. The focused `sonic2_options_netplay` CTest also checks the menu
overlay, both native input ports, exact menu/roster rollback replay, online
APPLY without disk writes, and the existing offline SAVE behavior.

These local relay checks cover boot ordering, state adoption, replay and cold
rematches. They do not replace the final two-machine Internet playtest or test
the launcher's graphical widgets or external relay deployment.
