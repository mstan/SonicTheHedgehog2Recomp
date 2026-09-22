"""Compare native save-menu navigation audio against the pre-sound executable.

Serial private-ROM fixtures; no speaker capture or bit-identical S3 claim.
Both builds receive identical controller inputs and a cleared campaign. The
pre-navigation stream must match; each horizontal/vertical move must produce
a changed PCM window through the real Z80/chip audio path. Unit tests cover
the complementary silent/blocked inputs and exact sound-queue writes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tomllib
import wave
import zlib

p = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'baseline-exe', 'rom', 's3k', 'out'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
root = a.out.resolve()
root.mkdir(parents=True, exist_ok=False)
identity = bytes.fromhex('193bc4064ce0daf27ea9e908ed246d87ec576cc294833badebb590b6ad8e8f6b')
assert hashlib.sha256(a.rom.read_bytes()).digest() == identity
save = bytearray(128)
save[:8] = b'S2SAVE\r\n'
struct.pack_into('>IIII', save, 8, 1, 128, 8, 1)
save[24:56] = identity
save[64:67] = bytes((2, 0, 0x7f))
struct.pack_into('>I', save, 60, zlib.crc32(save))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
           SDL_RENDER_DRIVER='software', GENESIS_STRICT_JSR_STACK='1')
lines = ['WAIT 600', 'PRESS START 2', 'WAIT 40', 'PRESS START 2', 'WAIT 100',
         'ASSERT_RAM8 FFF600 24']
for direction in ('RIGHT', 'LEFT', 'UP', 'DOWN'):
    lines += [f'DUMP_RAM {direction}.bin', 'WAIT 1', f'PRESS {direction} 2', 'WAIT 90']
# Use normal max-frame shutdown, not input-script EXIT: the latter returns
# before audio_wav_stop can finalize the WAV header in existing runners.
lines += ['ASSERT_RAM8 FFF600 24']
captures = {}
for name, exe in (('before', a.baseline_exe), ('after', a.exe)):
    out = root / name
    out.mkdir()
    shutil.copy2(exe, out / exe.name)
    shutil.copy2(exe.parent / 'SDL2.dll', out / 'SDL2.dll')
    (out / 'sonic2-party.ini').write_text(
        'slots=2\namy_enabled=0\ns3k_enabled=1\nsave_menu_enabled=1\n'
        f's3k_path={a.s3k.resolve().as_posix()}\nplayer1=sonic\nplayer2=tails\n'
        'player3=none\nplayer4=none\ncampaign_path=fixture.srm\n')
    (out / 'fixture.srm').write_bytes(save)
    (out / 'audio.input').write_text('\n'.join(lines) + '\n')
    with (out / 'run.log').open('w') as log:
        result = subprocess.run([str(out / exe.name), str(a.rom.resolve()), '--no-launcher',
            '--widescreen', 'off', '--input-script', 'audio.input', '--wav', 'audio.wav',
            '--max-frames', '1200', '--target-fps', '1000'], cwd=out, env=env,
            stdout=log, stderr=log, timeout=120,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    text = (out / 'run.log').read_text()
    assert result.returncode == 0 and '[DONE] 1200 frames completed' in text, (name, text[-1500:])
    assert not tomllib.loads((out / 'dispatch_misses.toml').read_text())['functions']['extra']
    frames = [int(n) for n in re.findall(r'DUMP_RAM requested at frame (\d+)', text)]
    assert len(frames) == 4, frames
    with wave.open(str(out / 'audio.wav'), 'rb') as wav:
        assert wav.getnframes() > wav.getframerate() * 15, 'Missing/truncated PCM capture'
        captures[name] = (wav.getparams(), wav.readframes(wav.getnframes()), frames)
    print('PASS native menu/audio run:', name, flush=True)

before, after = captures['before'], captures['after']
assert before[0] == after[0] and before[2] == after[2], 'Capture timelines differ'
params, _, frames = after
bytes_per_frame = params.nchannels * params.sampwidth
def offset(frame):
    return int(frame * params.framerate / 60) * bytes_per_frame
# A generous lead margin accounts for native audio preroll and frame batching.
assert before[1][:offset(frames[0]-30)] == after[1][:offset(frames[0]-30)], 'Audio changed before navigation'
windows = {}
for direction, frame in zip(('RIGHT', 'LEFT', 'UP', 'DOWN'), frames):
    lo, hi = offset(frame-15), offset(frame+60)
    old, new = before[1][lo:hi], after[1][lo:hi]
    assert old != new, f'No native PCM change for {direction}'
    windows[direction] = {'input_frame': frame, 'before_sha256': hashlib.sha256(old).hexdigest(),
                          'after_sha256': hashlib.sha256(new).hexdigest()}
(root / 'result.json').write_text(json.dumps({'sample_rate': params.framerate,
    'pre_navigation_identical': True, 'changed_windows': windows}, indent=2) + '\n')
print('PASS unchanged pre-navigation audio; all four movement windows changed; zero dispatch misses')
