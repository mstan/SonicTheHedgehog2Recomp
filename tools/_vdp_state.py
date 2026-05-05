"""Quick VDP-state diff between native (4380) and oracle (4381)."""
import socket, json
def cmd(host, port, name, **fields):
    s = socket.socket(); s.connect((host, port)); s.settimeout(10)
    msg = {'id': 1, 'cmd': name}; msg.update(fields)
    s.sendall((json.dumps(msg) + '\n').encode())
    buf = b''
    while b'\n' not in buf: buf += s.recv(65536)
    s.close()
    return json.loads(buf.split(b'\n')[0])

# Pause first so state is stable
cmd('127.0.0.1', 4380, 'pause')
cmd('127.0.0.1', 4381, 'pause')

n = cmd('127.0.0.1', 4380, 'vdp_state')
o = cmd('127.0.0.1', 4381, 'vdp_state')

# Diff every key whose value differs (skip id/ok)
keys = sorted((set(n) | set(o)) - {'id', 'ok'})
for k in keys:
    nv = n.get(k); ov = o.get(k)
    if nv != ov:
        print(f"  {k:<30}  oracle={ov!r}  native={nv!r}")
