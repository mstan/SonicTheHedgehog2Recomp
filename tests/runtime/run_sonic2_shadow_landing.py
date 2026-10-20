"""Muted six-party Shadow landing/spring visual regression on native terrain.

Cheats position the player and place a native horizontal spring. They never
set landing, spring-launch, animation outcome or success flags. Each case boots
fresh; every observed frame retains RAM and an image. No owner process is used.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tomllib

p = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'rom', 'config', 'out'):
    p.add_argument('--'+name, type=Path, required=True)
p.add_argument('--expect-failure', action='store_true')
a = p.parse_args()
root = a.out.resolve()
root.mkdir(parents=True, exist_ok=False)
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
           SDL_RENDER_DRIVER='software', GENESIS_STRICT_JSR_STACK='1')
start = ['WAIT 600', 'PRESS START 2', 'WAIT 40', 'PRESS START 2',
         'WAIT 340', 'ASSERT_RAM8 FFF600 0C']


def write(bits, at, value):
    return f'WRITE_RAM{bits} {0xFF0000+at:X} {value & ((1<<bits)-1):X}'


def place(x, y, speed):
    return [write(32, 0xB008, x<<16), write(32, 0xB00C, y<<16),
            write(16, 0xB010, speed), write(16, 0xB012, 0x100),
            write(16, 0xB014, speed), write(8, 0xB022, 2),
            write(8, 0xB01C, 0), write(8, 0xB03C, 0)]


results = {}
for case in ('ledge', 'spring'):
    out = root/case
    out.mkdir()
    exe = out/a.exe.name
    shutil.copy2(a.exe, exe)
    shutil.copy2(a.exe.parent/'SDL2.dll', out/'SDL2.dll')
    shutil.copy2(a.config, out/'sonic2-party.ini')
    (out/'settings.ini').write_text('[audio]\nvolume=0\n[video]\nfullscreen=0\n')
    # Position only while paused, so an in-flight native actor update cannot
    # overwrite the seed with a motion copy taken before the script ran.
    lines = start+['PRESS START 2', 'WAIT 12', 'ASSERT_RAM16 FFF63A 1']
    if case == 'spring':
        # Obj41 initializes the real spring and its force/collision handler.
        lines += ['ASSERT_RAM8 FFB900 0']
        lines += [write(32, 0xB900+i, 0) for i in range(0, 64, 4)]
        lines += [write(8, 0xB900, 0x41), write(16, 0xB908, 192),
                  write(16, 0xB90C, 656), write(8, 0xB928, 0x12)]
        lines += place(224, 640, 0x80)
    else:
        lines += place(128, 592, 0x300)
    lines += ['PRESS START 2', 'WAIT 2', 'ASSERT_RAM16 FFF63A 0']
    for tick in range(50):
        lines += ['WAIT 1', f'DUMP_RAM pose-{tick:02}.bin',
                  f'SCREENSHOT pose-{tick:02}.png']
    lines += ['EXIT']
    (out/'landing.input').write_text('\n'.join(lines)+'\n')
    with (out/'run.log').open('w') as log:
        run = subprocess.run([str(exe), str(a.rom.resolve()), '--no-launcher',
            '--input-script', 'landing.input', '--target-fps', '1000',
            '--max-frames', '2000', '--widescreen', 'off'], cwd=out, env=env,
            stdout=log, stderr=log, timeout=120,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    assert run.returncode == 0, (out/'run.log').read_text()[-2500:]
    assert '[input_script] EXIT' in (out/'run.log').read_text()
    assert not tomllib.loads((out/'dispatch_misses.toml').read_text()).get('functions', {}).get('extra')
    observations = []
    for tick in range(50):
        r = (out/f'pose-{tick:02}.bin').read_bytes()
        word = lambda at: int.from_bytes(r[at:at+2], 'big', signed=True)
        observations.append(dict(tick=tick, x=word(0xB008), y=word(0xB00C),
            vx=word(0xB010), vy=word(0xB012), inertia=word(0xB014),
            animation=r[0xB01C], frame=r[0xB01A], status=r[0xB022],
            spring_animation=r[0xB91C], spring_routine=r[0xB924],
            move_lock=word(0xB02E)))
    air = [o for o in observations if o['status']&6 == 2 and o['animation'] == 0]
    ground = [o for o in observations if not o['status']&2 and o['animation'] == 0]
    stale = [o for o in ground if 0xA9 <= o['frame'] <= 0xAF]
    results[case] = dict(airborne_ticks=len(air), grounded_ticks=len(ground),
                         stale_ground_ticks=len(stale), observations=observations)
    (root/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    assert air and any(0xA9 <= o['frame'] <= 0xAA for o in air), 'Fixture did not enter the falling script'
    assert ground, 'Fixture did not land with native Walk unchanged'
    if case == 'spring':
        assert any(o['spring_animation'] == 3 and o['vx'] >= 0x900 for o in ground), 'No native spring launch'
    if a.expect_failure:
        assert stale, 'Baseline no longer reproduces the stale airborne pose'
    else:
        assert not stale, stale
    print(case, 'REPRODUCED' if a.expect_failure else 'PASS',
          len(stale), 'stale ground poses', flush=True)
