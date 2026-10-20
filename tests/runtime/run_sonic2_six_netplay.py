"""One muted six-peer lobby/rollback smoke, using only tool-owned processes.

The isolated local server uses a fresh database. All guests run independent
movement/jump inputs; native outcomes and confirmed digests are checked. This
does not qualify Internet/NAT or different builds/architectures.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'rom', 'config', 'server', 'out'):
    p.add_argument('--'+name, type=Path, required=True)
a = p.parse_args()
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=False)
clean = {k: v for k, v in os.environ.items()
         if not k.startswith(('GENESIS_', 'RNET_', 'SDL_', 'LNG_', 'DISCORD_', 'JWT_'))}
server_dir = out/'server'
server_dir.mkdir()
with socket.socket() as s:
    s.bind(('127.0.0.1', 0))
    ws_port = s.getsockname()[1]
server_env = dict(clean, BIND_ADDR=f'127.0.0.1:{ws_port}', REQUIRE_AUTH='false',
                  DISCORD_REQUIRED='false', INPUT_RELAY_ENABLED='false',
                  INPUT_RELAY_ADVERTISE_HOST='127.0.0.1', INPUT_RELAY_ALLOW_LOOPBACK='1',
                  RUST_LOG='info', LOBBY_GAME_ALLOWLIST='')
logs = []
children = []
try:
    log = (server_dir/'run.log').open('w', encoding='utf-8')
    logs.append(log)
    server = subprocess.Popen([str(a.server.resolve())], cwd=server_dir, env=server_env,
        stdout=log, stderr=log, creationflags=subprocess.CREATE_NO_WINDOW)
    children.append(server)
    deadline = time.monotonic()+20
    while True:
        assert server.poll() is None, 'Own test lobby server exited'
        try:
            with socket.create_connection(('127.0.0.1', ws_port), timeout=.2):
                break
        except OSError:
            assert time.monotonic() < deadline, 'Own test lobby server did not listen'
            time.sleep(.1)
    peers = []
    for seat in range(6):
        work = out/f'peer{seat}'
        work.mkdir()
        exe = work/a.exe.name
        shutil.copy2(a.exe, exe)
        shutil.copy2(a.exe.parent/'SDL2.dll', work/'SDL2.dll')
        shutil.copy2(a.config, work/'sonic2-party.ini')
        (work/'settings.ini').write_text('[video]\nfullscreen=0\n[audio]\nvolume=0\n')
        script = (['WAIT 600', 'PRESS START 2', 'WAIT 40', 'PRESS START 2',
                   'WAIT 340'] if seat == 0 else ['WAIT 980'])
        script += ['ASSERT_RAM8 FFF600 0C', 'DUMP_RAM baseline.bin']
        if seat:
            script += ['HOLD RIGHT']
        for wave in range(1, 6):
            script += ['WAIT 10']
            if seat == wave:
                script += ['PRESS C 2']
            script += ['WAIT 10', f'DUMP_RAM jump-{wave}.bin', f'SCREENSHOT jump-{wave}.png']
        script += ['RELEASE']
        (work/'smoke.input').write_text('\n'.join(script)+'\n')
        env = dict(clean, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
            SDL_RENDER_DRIVER='software', GENESIS_STRICT_JSR_STACK='1',
            GENESIS_NO_LAUNCHER='1', GENESIS_NET_TIMELINE_EVERY='60',
            GENESIS_NET_DELAY='2', GENESIS_NET_PREDICTION='12',
            GENESIS_LOBBY_SELFTEST='host' if seat == 0 else 'guest',
            GENESIS_LOBBY_SELFTEST_NAME=f'SixSeat{seat+1}',
            GENESIS_LOBBY_SELFTEST_ROUNDS='1', GENESIS_LOBBY_SELFTEST_FRAMES='1200',
            GENESIS_LOBBY_SELFTEST_PLAYERS='6', GENESIS_LOBBY_SELFTEST_SPECTATORS='0',
            GENESIS_LOBBY_SELFTEST_LOBBY=f'genesis-six-{ws_port}',
            GENESIS_NET_LOBBY_URL=f'ws://127.0.0.1:{ws_port}',
            GENESIS_RB_FORCE_MISPREDICT='0')
        log = (work/'run.log').open('w', encoding='utf-8')
        logs.append(log)
        proc = subprocess.Popen([str(exe), str(a.rom.resolve()), '--no-launcher',
            '--input-script', 'smoke.input', '--widescreen', 'off'], cwd=work,
            env=env, stdout=log, stderr=log, creationflags=subprocess.CREATE_NO_WINDOW)
        children.append(proc)
        peers.append(proc)
        time.sleep(.4)
    deadline = time.monotonic()+120
    while any(proc.poll() is None for proc in peers) and time.monotonic() < deadline:
        time.sleep(.2)
    report = dict(peers=[], passed=False, limitations=['same machine/build only', 'Internet/NAT untested'])
    timeline = None
    slots = set()
    objects = (0xB000, 0xB040, 0xCFC0, 0xCF80, 0xCF40, 0xCF00)
    for index, proc in enumerate(peers):
        assert proc.poll() == 0, f'Peer {index} exited {proc.poll()}; see own run.log'
        work = out/f'peer{index}'
        text = (work/'run.log').read_text(encoding='utf-8', errors='replace')
        launch = re.search(r'fill_launch=1 slot=(\d+) players=6 session=\d+ .*occupied=3f', text)
        assert launch, f'Peer {index} did not enter a full six-seat lobby'
        slot = int(launch[1])
        slots.add(slot)
        summaries = re.findall(r'NETPLAY_DRIVER [^\n]+', text)
        assert len(summaries) == 1 and 'desyncs=0 ' in summaries[0] and 'refusal=none' in summaries[0], summaries
        assert int(re.search(r'sim=(\d+)', summaries[0])[1]) >= 1200, summaries
        hashes = [(int(t), h) for t, h in re.findall(r'TIMELINE t=(\d+) d=(\w+)', text) if int(t) < 1200]
        assert len(hashes) >= 18, 'Missing confirmed digest samples'
        if timeline is None:
            timeline = hashes
        else:
            assert hashes == timeline, f'Peer {index} differs from the host timeline'
        entry = dict(peer=index, slot=slot, summary=summaries[0], confirmed_samples=len(hashes))
        if index:
            at = objects[slot]
            before = (work/'baseline.bin').read_bytes()
            jumped = (work/f'jump-{index}.bin').read_bytes()
            x = lambda r: int.from_bytes(r[at+8:at+10], 'big')
            assert jumped[at+0x22]&2, f'Guest slot {slot+1} did not jump from its own network input'
            assert x(jumped) > x(before), f'Guest slot {slot+1} did not move from its own network input'
            entry.update(jumped=True, x_before=x(before), x_after=x(jumped))
        report['peers'].append(entry)
    assert slots == set(range(6)), slots
    report['passed'] = True
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2), flush=True)
finally:
    for proc in reversed(children):
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
    for log in logs:
        log.close()
