"""warp.py — one-command zone/act warp on a running Sonic 2 binary.

Usage:
    python tools/warp.py cpz1               # Chemical Plant act 1
    python tools/warp.py "mystic cave" 2    # names work too, act as 2nd arg
    python tools/warp.py mtz3               # MTZ act 3 (zone 5 internally)
    python tools/warp.py --list             # show all zones
    python tools/warp.py ehz2 --port 4381   # oracle build

Needs the TCP debug server (debug.ini next to the exe; native=4380).

Mechanism: pokes Current_ZoneAndAct ($FFFE10), clears the checkpoint flags
($FFFE30/31), then sets Level_Inactive_flag ($FFFE02) — the same restart
path death/level-select use, so the target level does a clean full reload.
The warp only fires while the level main loop is running (Game_Mode $0C);
from the SEGA/title/demo screens the script taps Start into EHZ 1 first
(title menu must be on the default "1 PLAYER") and warps from there.
Special stage / continue / 2P modes are not supported. HPZ is a leftover
zone and may crash.
"""
import argparse, json, socket, sys, time

GM_ADDR = "FFF600"        # Game_Mode (byte)
ZONEACT_ADDR = "FFFE10"   # Current_ZoneAndAct (word: zone<<8 | act0)
RESTART_ADDR = "FFFE02"   # Level_Inactive_flag (word)
CHECKPOINT_ADDR = "FFFE30"  # Last_star_pole_hit P1 + P2 (2 bytes)

GM_SEGA, GM_TITLE, GM_DEMO, GM_LEVEL = 0x00, 0x04, 0x08, 0x0C

# name: (zone_id, act_count, aliases)
ZONES = {
    "EHZ": (0x00, 2, ("emeraldhill",)),
    "CPZ": (0x0D, 2, ("chemicalplant",)),
    "ARZ": (0x0F, 2, ("aquaticruin",)),
    "CNZ": (0x0C, 2, ("casinonight",)),
    "HTZ": (0x07, 2, ("hilltop",)),
    "MCZ": (0x0B, 2, ("mysticcave",)),
    "OOZ": (0x0A, 2, ("oilocean",)),
    "MTZ": (0x04, 3, ("metropolis",)),  # act 3 is zone 0x05 act 1
    "SCZ": (0x10, 1, ("skychase",)),
    "WFZ": (0x06, 1, ("wingfortress",)),
    "DEZ": (0x0E, 1, ("deathegg",)),
    "HPZ": (0x08, 1, ("hiddenpalace",)),  # leftover data — may crash
}


def parse_target(words):
    """Accept 'cpz1', 'cpz 1', 'chemical plant 2', 'MTZ'. Returns (name, act1based)."""
    text = "".join(words).lower().replace(" ", "").replace("_", "").replace("-", "")
    act = 1
    if text and text[-1].isdigit():
        act = int(text[-1])
        text = text[:-1]
    text = text.replace("zone", "").replace("act", "")
    for name, (_, acts, aliases) in ZONES.items():
        if text == name.lower() or text in aliases:
            if not 1 <= act <= acts:
                sys.exit(f"error: {name} has {acts} act(s), asked for act {act}")
            return name, act
    sys.exit(f"error: unknown zone '{ ' '.join(words) }' — try --list")


def zoneact_word(name, act):
    zone, _, _ = ZONES[name]
    if name == "MTZ" and act == 3:
        return 0x0500
    return (zone << 8) | (act - 1)


class Client:
    def __init__(self, port):
        self.s = socket.socket()
        self.s.connect(("127.0.0.1", port))
        self.s.settimeout(10.0)
        self.nid = 1

    def cmd(self, name, **kw):
        msg = {"id": self.nid, "cmd": name}
        self.nid += 1
        msg.update(kw)
        self.s.sendall((json.dumps(msg) + "\n").encode())
        buf = b""
        while b"\n" not in buf:
            ch = self.s.recv(1 << 20)
            if not ch:
                raise RuntimeError("connection closed")
            buf += ch
        r = json.loads(buf.split(b"\n", 1)[0].decode())
        if "error" in r:
            raise RuntimeError(f"{name}: {r['error']}")
        return r

    def read_byte(self, addr):
        return int(self.cmd("read_memory", addr=addr, size=1)["hex"], 16)

    def read_word(self, addr):
        return int(self.cmd("read_memory", addr=addr, size=2)["hex"], 16)

    def write_hex(self, addr, hexstr):
        self.cmd("write_memory", addr=addr, hex=hexstr)

    def tap_start(self):
        self.cmd("set_input", keys="80")
        time.sleep(0.15)
        self.cmd("set_input", keys="off")


def main():
    ap = argparse.ArgumentParser(description="Warp a running Sonic 2 to a zone/act.")
    ap.add_argument("target", nargs="*", help="zone name + optional act (cpz1, 'mystic cave 2')")
    ap.add_argument("--port", type=int, default=4380, help="debug server port (default 4380)")
    ap.add_argument("--list", action="store_true", help="list zones and exit")
    args = ap.parse_args()

    if args.list or not args.target:
        for name, (zone, acts, aliases) in ZONES.items():
            extra = "  (leftover — may crash)" if name == "HPZ" else ""
            print(f"  {name}  zone 0x{zone:02X}  acts 1-{acts}  ({aliases[0]}){extra}")
        return 0 if args.list else 1

    name, act = parse_target(args.target)
    target = zoneact_word(name, act)
    if name == "HPZ":
        print("warning: HPZ is leftover data and may crash")

    try:
        c = Client(args.port)
    except OSError as e:
        sys.exit(f"error: can't connect to 127.0.0.1:{args.port} ({e}) — "
                 "is the game running with debug.ini next to the exe?")

    print(f"warping to {name} act {act} (Current_ZoneAndAct = {target:04X})")

    # Get to a running level (GM 0x0C); from menus, tap Start toward EHZ 1.
    deadline = time.time() + 60.0
    last_tap = 0.0
    warped = False
    while time.time() < deadline:
        gm = c.read_byte(GM_ADDR)
        base = gm & 0x7F
        if not warped:
            if gm == GM_LEVEL:
                c.write_hex(ZONEACT_ADDR, f"{target:04X}")
                c.write_hex(CHECKPOINT_ADDR, "0000")
                c.write_hex(RESTART_ADDR, "0001")
                warped = True
                print("restart flag set, waiting for reload...")
            elif gm in (GM_TITLE, GM_DEMO) and time.time() - last_tap > 1.0:
                print(f"Game_Mode 0x{gm:02X} — tapping Start")
                c.tap_start()
                last_tap = time.time()
            elif base not in (GM_SEGA, GM_TITLE, GM_DEMO, GM_LEVEL):
                sys.exit(f"error: can't warp from Game_Mode 0x{gm:02X} "
                         "(special stage/continue/2P?) — get to the title screen or a level")
        else:
            if gm == GM_LEVEL and c.read_word(ZONEACT_ADDR) == target:
                print(f"done: {name} act {act} loaded and running")
                return 0
        time.sleep(0.2)

    sys.exit("error: timed out waiting for the warp to complete")


if __name__ == "__main__":
    sys.exit(main())
