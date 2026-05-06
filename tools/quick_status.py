"""quick_status.py — one-shot snapshot of both binaries' Vint_runcount,
wall frame, and ring extents."""
import socket, json, sys

def query(port, label):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    s.settimeout(5.0)

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

    fi = cmd("frame_info")
    cur = fi.get("current_frame", fi.get("frame", 0))
    cap = 600
    lo = max(0, cur - cap + 1)
    ts = cmd("frame_timeseries", field="wram32[FE0C]", **{"from": cur - 5, "to": cur})
    recent_vint = ts.get("values") or []
    print(f"--- {label} (port {port}) ---")
    print(f"  current_frame={cur}  ring=[{lo}..{cur}]")
    print(f"  Vint_runcount tail (frames {cur-5}..{cur}): {recent_vint}")
    s.close()
    return cur, recent_vint

n_cur, n_vint = query(4380, "native")
o_cur, o_vint = query(4381, "oracle")
print()
print(f"native latest Vint = {n_vint[-1] if n_vint else None}")
print(f"oracle latest Vint = {o_vint[-1] if o_vint else None}")
if n_vint and o_vint and n_vint[-1] is not None and o_vint[-1] is not None:
    print(f"oracle / native = {o_vint[-1] / max(n_vint[-1], 1):.3f}")
    print(f"oracle - native = {o_vint[-1] - n_vint[-1]}")
