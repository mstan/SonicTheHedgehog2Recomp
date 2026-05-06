"""vbla_breakdown.py — query the always-on vbla-fire ring on native and
report what fraction of fires were SUPPRESSED (imask>=6) vs THRESHOLD."""
import socket, json, sys

def query_all(port, label):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    s.settimeout(20.0)

    def cmd(name, **kw):
        msg = {"id": 1, "cmd": name}
        msg.update(kw)
        s.sendall((json.dumps(msg) + "\n").encode())
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(1 << 20)
            if not chunk: raise RuntimeError("closed")
            buf += chunk
        return json.loads(buf.split(b"\n", 1)[0].decode())

    all_entries = []
    start = 0
    while True:
        r = cmd("rdb_vbla_dump", start=start, count=5000)
        if not r.get("ok"):
            raise RuntimeError(f"{label}: {r}")
        log = r.get("log", [])
        all_entries.extend(log)
        if r.get("done"):
            break
        start += len(log)
        if len(log) == 0:
            break
    s.close()
    return all_entries

native = query_all(4380, "native")
oracle = query_all(4381, "oracle")

def breakdown(label, entries):
    n = len(entries)
    by_reason = {}
    for e in entries:
        r = e.get("reason", -1)
        by_reason[r] = by_reason.get(r, 0) + 1
    print(f"--- {label} ({n} fires recorded) ---")
    REASON_NAMES = {0: "THRESHOLD", 1: "SUPPRESSED", 2: "FORCED"}
    for r in sorted(by_reason):
        pct = 100.0 * by_reason[r] / n if n else 0
        print(f"  reason={r} ({REASON_NAMES.get(r,'?')}): {by_reason[r]} ({pct:.1f}%)")
    if entries:
        wall_first = entries[0].get("wall", 0)
        wall_last  = entries[-1].get("wall", 0)
        print(f"  wall span: {wall_first}..{wall_last} ({wall_last-wall_first} frames)")

breakdown("NATIVE", native)
breakdown("ORACLE", oracle)
