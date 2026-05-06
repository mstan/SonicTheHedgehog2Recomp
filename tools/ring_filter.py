"""ring_filter.py — pull the WRAM store ring from one binary and report
all stores hitting a given byte address.

Usage:  python tools/ring_filter.py PORT ADDR [ADDR...]
        addresses are 24-bit 68K bus addrs in hex (e.g., FFF62A FFFE0C)
        port: 4380 (native) or 4381 (oracle)

Reports per address: count, value distribution (top 8), wall-frame
coverage (first..last), recent samples with the storing function.
"""
import socket, json, sys, collections

def cli(port):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    s.settimeout(60.0)
    nid = [1]
    def cmd(name, **kw):
        msg = {"id": nid[0], "cmd": name}; nid[0]+=1; msg.update(kw)
        s.sendall((json.dumps(msg)+"\n").encode())
        buf = b""
        while b"\n" not in buf:
            ch = s.recv(1<<20)
            if not ch: raise RuntimeError("closed")
            buf += ch
        return json.loads(buf.split(b"\n",1)[0].decode())
    return s, cmd

port = int(sys.argv[1])
targets = {int(a, 16) for a in sys.argv[2:]}
print(f"port={port} watching addrs: {[f'{a:06X}' for a in sorted(targets)]}")

s, cmd = cli(port)
cnt = cmd("rdb_count")
total = cnt.get("count", 0)
ranges = cnt.get("ranges", [])
print(f"ring snapshot: count={total} ranges={ranges}")

def hx(v):
    if isinstance(v, str):
        return int(v, 16)
    return int(v)

CHUNK = 100000
hits_by_addr = collections.defaultdict(list)
start = 0
fetched = 0
while start < total:
    take = min(CHUNK, total - start)
    r = cmd("rdb_dump", start=start, count=take)
    if not r.get("ok"):
        raise RuntimeError(r)
    log = r.get("log", [])
    for e in log:
        a = hx(e.get("adr", 0))
        if a in targets:
            hits_by_addr[a].append(e)
    fetched += len(log)
    if not log:
        break
    start += len(log)
    if r.get("done"):
        break
print(f"scanned {fetched} entries from ring")
s.close()

for adr in sorted(hits_by_addr):
    hits = hits_by_addr[adr]
    print(f"\n========== ${adr:06X}  ({len(hits)} writes) ==========")
    val_counts = collections.Counter(hx(h.get("val", 0)) for h in hits)
    print(f"value distribution: {dict((f'0x{k:02X}', v) for k,v in val_counts.most_common(8))}")
    frames = [h.get("f", 0) for h in hits]
    if frames:
        print(f"frame range: {min(frames)}..{max(frames)}  (span={max(frames)-min(frames)})")
        print(f"recent writes (last 12):")
        for h in hits[-12:]:
            print(f"  f={h.get('f','?')} vint={h.get('vint','?')} adr={h.get('adr','?')}"
                  f" val={h.get('val','?')} w={h.get('w','?')}"
                  f" func={h.get('func','?')} caller={h.get('caller','?')}")
