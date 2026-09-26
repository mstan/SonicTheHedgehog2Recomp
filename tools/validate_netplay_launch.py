#!/usr/bin/env python3
"""Exercise pre-boot online launch, rollback and rematches in isolated copies.

Requires a running recomp-net-server with its UDP input relay enabled. Supply a
locally owned ROM; neither the ROM nor the test outputs belong in a release ZIP.
This calls the launcher's real lobby callbacks before machine initialization.
It does not automate the graphical widgets or validate Internet NAT traversal.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import time
import zipfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package', type=Path, required=True, help='Windows release ZIP')
    ap.add_argument('--exe', type=Path, help='Optional replacement executable to test')
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True, help='New output directory')
    ap.add_argument('--lobby-url', required=True, help='Use an isolated test server')
    ap.add_argument('--players', type=int, choices=(2, 3, 4), default=2)
    ap.add_argument('--rounds', type=int, default=1)
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--timeout', type=int, default=90)
    ap.add_argument('--latency', type=int, default=0)
    ap.add_argument('--loss', type=int, default=0)
    ap.add_argument('--mispredict', type=int, default=0)
    ap.add_argument('--host-video', default='off')
    ap.add_argument('--guest-video', default='off')
    ap.add_argument('--host-roster', default='sonic,tails')
    ap.add_argument('--guest-roster', default='sonic,tails')
    ap.add_argument('--gameplay', action='store_true')
    args = ap.parse_args()
    if args.rounds < 1 or args.frames < 120 or args.timeout < 1:
        ap.error('rounds/timeout must be positive and frames must be at least 120')
    if not args.rom.is_file() or not args.package.is_file():
        ap.error('ROM and package must exist')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    lobby = 's2-launch-' + secrets.token_hex(8)  # Fits the lobby's 31-character name.
    processes, logs, originals = [], [], []
    failures, results, timelines = [], [], []
    try:
        for seat in range(args.players):
            work = out / f'seat{seat}'
            work.mkdir()
            with zipfile.ZipFile(args.package) as package:
                package.extractall(work)
            exe = work / 'SonicTheHedgehog2Recomp.exe'
            if args.exe:
                shutil.copy2(args.exe, exe)
            roster = (args.host_roster if seat == 0 else args.guest_roster).split(',')
            party = f'version=1\nslots={len(roster)}\n'
            party += ''.join(f'player{p+1}={c}\n' for p, c in enumerate(roster))
            video = args.host_video if seat == 0 else args.guest_video
            settings = f'[mods.widescreen]\nenabled={int(video != "off")}\naspect={video}\n'
            for name, value in [('settings.ini', settings), ('sonic2-party.ini', party)]:
                (work / name).write_text(value, encoding='utf-8')
            originals.append({name: (work / name).read_bytes() for name in ('settings.ini', 'sonic2-party.ini')})
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(('GENESIS_', 'RNET_', 'SDL_', 'LNG_'))}
            env.update(SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy',
                       GENESIS_NO_LAUNCHER='1', GENESIS_RUN_DONE='1', GENESIS_NET_TIMELINE_EVERY='60',
                       GENESIS_LOBBY_SELFTEST='host' if seat == 0 else 'guest',
                       GENESIS_LOBBY_SELFTEST_PREBOOT='1', GENESIS_LOBBY_SELFTEST_NAME=f'seat{seat}',
                       GENESIS_LOBBY_SELFTEST_LOBBY=lobby, GENESIS_LOBBY_SELFTEST_PLAYERS=str(args.players),
                       GENESIS_LOBBY_SELFTEST_ROUNDS=str(args.rounds), GENESIS_LOBBY_SELFTEST_FRAMES=str(args.frames),
                       GENESIS_NET_LOBBY_URL=args.lobby_url,
                       GENESIS_RB_FORCE_MISPREDICT=str(args.mispredict if seat == 1 else 0),
                       RNET_SIM_LATENCY_MS=str(args.latency), RNET_SIM_LOSS_PCT=str(args.loss),
                       RNET_SIM_SEED=str(314159 + seat))
            command = [str(exe), str(args.rom.resolve()), '--no-launcher']
            if args.gameplay:
                script = Path(__file__).parent / f'netplay_campaign_{"host" if seat == 0 else "guest"}.input'
                # Reaching this capture proves the input script passed the
                # native level-loading transition rather than idling at title.
                text = script.read_text(encoding='utf-8').replace('HOLD RIGHT',
                    'SCREENSHOT gameplay.png\nDUMP_RAM gameplay.ram\nHOLD RIGHT')
                (work / 'input.txt').write_text(text, encoding='utf-8')
                command += ['--input-script', str(work / 'input.txt')]
            log = (work / 'process.log').open('w', encoding='utf-8')
            logs.append(log)
            processes.append(subprocess.Popen(command, cwd=work, env=env, stdout=log,
                stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0))
            if seat == 0:
                time.sleep(1)
        deadline = time.monotonic() + args.timeout
        while any(p.poll() is None for p in processes) and time.monotonic() < deadline:
            time.sleep(0.2)
        for seat, process in enumerate(processes):
            timed_out = process.poll() is None
            if timed_out:
                process.kill()
            code = process.wait()
            logs[seat].flush()
            work = out / f'seat{seat}'
            content = (work / 'process.log').read_text(encoding='utf-8', errors='replace')
            rows = re.findall(r'NETPLAY_DRIVER .*', content)
            if code != 0 or timed_out:
                failures.append(f'seat {seat}: exit={code}, timeout={timed_out}')
            if '[lobby-selftest] preboot launcher ordering' not in content:
                failures.append(f'seat {seat}: build lacks preboot regression mode')
            if len(rows) != args.rounds:
                failures.append(f'seat {seat}: completed {len(rows)} rounds, expected {args.rounds}')
            for row in rows:
                if ('desyncs=0 ' not in row or 'refusal=none' not in row
                        or int(re.search(r'sim=(\d+)', row)[1]) < args.frames):
                    failures.append(f'seat {seat}: unhealthy round: {row}')
            if args.mispredict and not any(int(re.search(r'episodes=(\d+)', row)[1]) > 0 for row in rows):
                failures.append(f'seat {seat}: rollback was not exercised')
            for name, original in originals[seat].items():
                if (work / name).read_bytes() != original:
                    failures.append(f'seat {seat}: local {name} was changed')
            if args.gameplay:
                ram = work / 'gameplay.ram'
                if not ram.exists() or len(ram.read_bytes()) != 65536 or ram.read_bytes()[0xF600] != 0x0C:
                    failures.append(f'seat {seat}: did not reach level gameplay')
            # Final drain samples can be emitted by only one peer; compare the
            # interior confirmed timeline in order, including repeated rounds.
            hashes = [(int(t), h) for t, h in re.findall(r'TIMELINE t=(\d+) d=(\w+)', content)
                      if int(t) < args.frames]
            if len(hashes) != args.rounds * ((args.frames - 1) // 60):
                failures.append(f'seat {seat}: incomplete confirmed timeline')
            timelines.append(hashes)
            results.append(dict(seat=seat, exit=code, timed_out=timed_out, rounds=rows,
                                log=str(work / 'process.log')))
        for seat in range(1, len(timelines)):
            if timelines[seat] != timelines[0]:
                failures.append(f'seat {seat}: confirmed state hashes differ from host')
        verdict = dict(passed=not failures, failures=failures, peers=results)
        (out / 'result.json').write_text(json.dumps(verdict, indent=2), encoding='utf-8')
        print(json.dumps(verdict, indent=2))
        return int(bool(failures))
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait()
        for log in logs:
            log.close()


if __name__ == '__main__':
    raise SystemExit(main())
