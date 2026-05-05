"""Pause both binaries ASAP and report their frames + a couple of RAM diffs."""
import socket, json, sys

def cmd(host, port, name, **fields):
    s = socket.socket()
    s.connect((host, port))
    s.settimeout(10)
    msg = {'id': 1, 'cmd': name}
    msg.update(fields)
    s.sendall((json.dumps(msg) + '\n').encode())
    buf = b''
    while b'\n' not in buf:
        buf += s.recv(65536)
    s.close()
    return json.loads(buf.split(b'\n')[0])

cmd('127.0.0.1', 4380, 'pause')
cmd('127.0.0.1', 4381, 'pause')
n_frame = cmd('127.0.0.1', 4380, 'frame_info').get('current_frame')
o_frame = cmd('127.0.0.1', 4381, 'frame_info').get('current_frame')
print(f'native frame: {n_frame}')
print(f'oracle frame: {o_frame}')
print(f'frame_skew  : {n_frame - o_frame:+d}')

# Read a few RAM probes from each
WATCH = [
    ('game_mode',     0xFFF600, 1),
    ('vint_routine',  0xFFF62A, 1),
    ('vint_runcount', 0xFFFE0C, 4),
    ('p1_obj_id',     0xFFD000, 1),
    ('p1_x',          0xFFD008, 2),
    ('scroll_x',      0xFFF700, 2),
]
print(f"\n{'name':<14}  {'oracle':>10}  {'native':>10}  diff?")
for name, addr, w in WATCH:
    n = cmd('127.0.0.1', 4380, 'read_ram', addr=addr, len=w)
    o = cmd('127.0.0.1', 4381, 'read_ram', addr=addr, len=w)
    n_data = n.get('data') or n.get('bytes') or '0'
    o_data = o.get('data') or o.get('bytes') or '0'
    n_v = int(n_data.replace(' ', ''), 16) if n_data else 0
    o_v = int(o_data.replace(' ', ''), 16) if o_data else 0
    flag = '<-- DIFF' if n_v != o_v else ''
    print(f"{name:<14}  0x{o_v:08X}  0x{n_v:08X}  {flag}")
