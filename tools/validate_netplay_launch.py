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
    ap.add_argument('--runtime-dir', type=Path, required=True,
                    help='Dedicated reusable test installation; keep its executable paths fixed for Windows Firewall')
    ap.add_argument('--lobby-url', required=True, help='Use an isolated test server')
    ap.add_argument('--players', type=int, choices=(2, 3, 4), default=2)
    ap.add_argument('--rounds', type=int, default=1)
    ap.add_argument('--then-offline', action='store_true', help='Also verify the same processes can cold-reset back to local play')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--timeout', type=int, default=90)
    ap.add_argument('--latency', type=int, default=0)
    ap.add_argument('--loss', type=int, default=0)
    ap.add_argument('--mispredict', type=int, default=0)
    ap.add_argument('--host-video', default='off')
    ap.add_argument('--guest-video', default='off')
    ap.add_argument('--host-roster', default='sonic,tails')
    ap.add_argument('--guest-roster', default='sonic,tails')
    ap.add_argument('--amy', type=Path, help='Private Amy in Sonic 2 Rev 1.7.1 donor')
    ap.add_argument('--s3k', type=Path, help='Private combined Sonic 3 & Knuckles donor')
    ap.add_argument('--gameplay', action='store_true')
    ap.add_argument('--campaign-views', action='store_true',
                    help='Two peers separate for over 300 ticks; capture their independent campaign views')
    ap.add_argument('--options', action='store_true',
                    help='Select four characters through online Options, then enter gameplay')
    args = ap.parse_args()
    if args.campaign_views:
        if args.players != 2 or args.rounds != 1 or args.mispredict or args.options:
            ap.error('--campaign-views uses one two-peer round with normal inputs')
        args.gameplay = True
    if args.rounds < 1 or args.frames < 120 or args.timeout < 1:
        ap.error('rounds/timeout must be positive and frames must be at least 120')
    if not args.rom.is_file() or not args.package.is_file():
        ap.error('ROM and package must exist')
    for donor in (args.amy, args.s3k):
        if donor and not donor.is_file():
            ap.error(f'Donor must exist: {donor}')
    if args.options:
        if not args.amy or not args.s3k or args.host_roster != 'sonic,tails' or args.rounds != 1:
            ap.error('--options requires both donors, the default host roster, and one round')
        args.gameplay = True
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime_dir.resolve()
    runtime.mkdir(parents=True, exist_ok=True)
    # One test session at a time, reusing the same executable paths. Results
    # remain separate, without registering fresh applications with Firewall.
    lock = runtime / '.netplay-validation.lock'
    try:
        lock_file = lock.open('x')
    except FileExistsError:
        ap.error(f'Test runtime already in use: {lock}')
    lock_file.write(str(os.getpid()))
    lock_file.close()
    lobby = 's2-launch-' + secrets.token_hex(8)  # Fits the lobby's 31-character name.
    processes, logs, originals = [], [], []
    failures, results, timelines = [], [], []
    try:
        for seat in range(args.players):
            work = out / f'seat{seat}'
            work.mkdir()
            install = runtime / f'seat{seat}'
            install.mkdir(exist_ok=True)
            with zipfile.ZipFile(args.package) as package:
                package.extractall(install)
            exe = install / 'SonicTheHedgehog2Recomp.exe'
            if args.exe:
                shutil.copy2(args.exe, exe)
            roster = (args.host_roster if seat == 0 else args.guest_roster).split(',')
            party = f'version=1\nslots={len(roster)}\n'
            party += ''.join(f'player{p+1}={c}\n' for p, c in enumerate(roster))
            party += f'amy_enabled={int(bool(args.amy))}\ns3k_enabled={int(bool(args.s3k))}\nsave_menu_enabled=0\n'
            if args.amy:
                party += f'amy_path={args.amy.resolve().as_posix()}\n'
            if args.s3k:
                party += f's3k_path={args.s3k.resolve().as_posix()}\n'
            video = args.host_video if seat == 0 else args.guest_video
            settings = f'[mods.widescreen]\nenabled={int(video != "off")}\naspect={video}\n'
            for name, value in [('settings.ini', settings), ('sonic2-party.ini', party)]:
                (install / name).write_text(value, encoding='utf-8')
                shutil.copy2(install / name, work / name)
            originals.append({name: (work / name).read_bytes() for name in ('settings.ini', 'sonic2-party.ini')})
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(('GENESIS_', 'RNET_', 'SDL_', 'LNG_'))}
            env.update(SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy',
                       LNG_TEST_HIDDEN='1',
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
            if args.then_offline:
                env['GENESIS_LOBBY_SELFTEST_THEN_OFFLINE'] = '1'
            if args.gameplay:
                scenario = 'views' if args.campaign_views else 'options' if args.options else 'campaign'
                script = Path(__file__).parent / f'netplay_{scenario}_{"host" if seat == 0 else "guest"}.input'
                # Reaching this capture proves the input script passed the
                # native level-loading transition rather than idling at title.
                text = script.read_text(encoding='utf-8').replace('HOLD RIGHT',
                    'SCREENSHOT gameplay.png\nDUMP_RAM gameplay.ram\nHOLD RIGHT')
                for capture in ('options.png', 'gameplay.png', 'gameplay.ram', 'apart.png', 'apart.ram', 'retained.png', 'retained.ram'):
                    text = text.replace(capture, (work / capture).as_posix())
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
            if '[session] cold reset FAILED' in content:
                failures.append(f'seat {seat}: cold reset failed')
            if args.then_offline and ('offline Play after' not in content or 'RUN_DONE' not in content):
                failures.append(f'seat {seat}: did not finish local play after leaving netplay')
            if len(rows) != args.rounds:
                failures.append(f'seat {seat}: completed {len(rows)} rounds, expected {args.rounds}')
            expected_game = f'game=r={args.host_roster} m={int(bool(args.amy))}{int(bool(args.s3k))}'
            game_configs = re.findall(r'^game=.*$', content, re.MULTILINE)
            if len(game_configs) < args.rounds or any(c != expected_game for c in game_configs):
                failures.append(f'seat {seat}: host roster/donor settings were not adopted: {game_configs}')
            for row in rows:
                if ('desyncs=0 ' not in row or 'refusal=none' not in row
                        or int(re.search(r'sim=(\d+)', row)[1]) < args.frames):
                    failures.append(f'seat {seat}: unhealthy round: {row}')
            if args.mispredict and not any(int(re.search(r'episodes=(\d+)', row)[1]) > 0 for row in rows):
                failures.append(f'seat {seat}: rollback was not exercised')
            for name, original in originals[seat].items():
                if (runtime / f'seat{seat}' / name).read_bytes() != original:
                    failures.append(f'seat {seat}: local {name} was changed')
            if args.gameplay:
                ram = work / 'gameplay.ram'
                if not ram.exists() or len(ram.read_bytes()) != 65536 or ram.read_bytes()[0xF600] != 0x0C:
                    failures.append(f'seat {seat}: did not reach level gameplay')
                elif args.options:
                    data = ram.read_bytes()
                    if [data[p] for p in (0xB000, 0xB040, 0xCFC0, 0xCF80)] != [1, 2, 1, 1]:
                        failures.append(f'seat {seat}: online Options did not spawn all four actors')
                    if not (work / 'options.png').is_file():
                        failures.append(f'seat {seat}: missing online Options capture')
            if args.campaign_views:
                for phase in ('apart', 'retained'):
                    capture = work / f'{phase}.ram'
                    if not capture.exists() or len(capture.read_bytes()) != 65536:
                        failures.append(f'seat {seat}: missing {phase} capture')
                        continue
                    data = capture.read_bytes()
                    word = lambda at: int.from_bytes(data[at:at+2], 'big')
                    if data[0xF600] != 0x0C or word(0xFFD8):
                        failures.append(f'seat {seat}: {phase} is not campaign gameplay')
                    if abs(word(0xB008)-word(0xB048)) < 400 or data[0xB064] != 2 or word(0xF708) != 6:
                        failures.append(f'seat {seat}: {phase} companion did not remain independently playable '
                                        f'(x={word(0xB008)},{word(0xB048)}, routine={data[0xB064]}, AI={word(0xF708)})')
                    if not (work / f'{phase}.png').is_file():
                        failures.append(f'seat {seat}: missing {phase} screen')
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
        lock.unlink()


if __name__ == '__main__':
    raise SystemExit(main())
