"""Dump Normal_palette at $FFFB00 from native + oracle, plus current CRAM."""
import socket, json
def cmd(host, port, name, **fields):
    s = socket.socket(); s.connect((host, port)); s.settimeout(10)
    msg = {'id': 1, 'cmd': name}; msg.update(fields)
    s.sendall((json.dumps(msg) + '\n').encode())
    buf = b''
    while b'\n' not in buf: buf += s.recv(65536)
    s.close()
    return json.loads(buf.split(b'\n')[0])

cmd('127.0.0.1', 4380, 'pause')
cmd('127.0.0.1', 4381, 'pause')

# Frame
print("native frame:", cmd('127.0.0.1', 4380, 'frame_info').get('current_frame'))
print("oracle frame:", cmd('127.0.0.1', 4381, 'frame_info').get('current_frame'))

# Read 128 bytes (full palette) at $FFFB00
n_pal = cmd('127.0.0.1', 4380, 'read_ram', addr=0xFFFB00, len=128)
o_pal = cmd('127.0.0.1', 4381, 'read_ram', addr=0xFFFB00, len=128)
n_data = (n_pal.get('data') or '').replace(' ', '')
o_data = (o_pal.get('data') or '').replace(' ', '')

print("\n--- Normal_palette at $FFFFFB00 (128 bytes) ---")
print("native:", n_data[:64], "..." if len(n_data) > 64 else "")
print("oracle:", o_data[:64], "..." if len(o_data) > 64 else "")
print("match:", n_data == o_data)

# Read CRAM
n_cram = cmd('127.0.0.1', 4380, 'read_cram').get('hex', '')
o_cram = cmd('127.0.0.1', 4381, 'read_cram').get('hex', '')
print("\n--- CRAM (128 bytes / 64 colors) ---")
print("native:", n_cram[:64])
print("oracle:", o_cram[:64])
print("match:", n_cram == o_cram)
