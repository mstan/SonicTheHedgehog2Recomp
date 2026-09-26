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
repository. Each peer gets a separate executable, settings, roster and logs.

```powershell
python tools/validate_netplay_launch.py --package SonicTheHedgehog2Recomp-v0.8.0-rc2-win64.zip --rom C:/ROMs/sonic2.bin --out validation/rematches --lobby-url ws://127.0.0.1:18765 --rounds 3 --latency 60 --loss 1 --mispredict 45
python tools/validate_netplay_launch.py --package SonicTheHedgehog2Recomp-v0.8.0-rc2-win64.zip --rom C:/ROMs/sonic2.bin --out validation/four-player --lobby-url ws://127.0.0.1:18765 --players 4 --frames 2400 --gameplay --host-video 16:9 --host-roster sonic,tails,sonic,tails --guest-roster tails,sonic --latency 40 --mispredict 90
```

`--exe` optionally replaces the ZIP's executable for development. The script
requires clean exits, all requested rounds, matching confirmed state digests,
no desync/refusal, real rollback episodes when requested, and unchanged local
settings files. Gameplay runs also capture the level and verify its RAM mode.
Results and peer logs remain under `--out`; a failure returns a nonzero status.

These local relay checks cover boot ordering, state adoption, replay and cold
rematches. They do not replace the final two-machine Internet playtest or test
the graphical widgets, external relay deployment, or donor-backed characters.
