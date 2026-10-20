"""Muted Mighty spike QA: compare native physics and retain visual evidence.

RAM positioning and Super prerequisites are explicit fixtures. They do not
claim natural route/emerald collection coverage. No success flags are seeded.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tomllib

p = argparse.ArgumentParser(description=__doc__)
for name in ("exe", "rom", "out"):
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--character", choices=("mighty",), default="mighty")
p.add_argument("--donor", type=Path, help="Verified raw Megamix v3 donor for Mighty")
a = p.parse_args()
character = a.character
if character == "mighty" and not a.donor:
    p.error("--donor is required for Mighty")
root = a.out.resolve()
root.mkdir(parents=True, exist_ok=False)
env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
           SDL_RENDER_DRIVER="software", GENESIS_STRICT_JSR_STACK="1")
results = []
start = ["WAIT 600", "PRESS START 2", "WAIT 40", "PRESS START 2", "WAIT 340",
         "ASSERT_RAM8 FFF600 0C"]


def capture(name):
    return [f"SCREENSHOT {name}.png", f"SAVE_STATE {name}.state", "WAIT 8"]


def run(name, roster, lines, widescreen="off"):
    out = root / name
    out.mkdir()
    exe = out / a.exe.name
    shutil.copy2(a.exe, exe)
    shutil.copy2(a.exe.parent / "SDL2.dll", out / "SDL2.dll")
    (out / "sonic2-party.ini").write_text("slots=4\namy_enabled=0\ns3k_enabled=0\n" +
        (f"mighty_enabled=1\nmighty_path={a.donor.resolve().as_posix()}\n" if a.donor else "") +
        "".join(f"player{i+1}={c}\n" for i, c in enumerate(roster)))
    (out / "check.input").write_text("\n".join(lines + ["EXIT"]) + "\n")
    with (out / "run.log").open("w") as log:
        proc = subprocess.run([str(exe), str(a.rom.resolve()), "--no-launcher",
            "--widescreen", widescreen, "--input-script", "check.input",
            "--max-frames", "6500", "--target-fps", "1000"], cwd=out,
            env=env, stdout=log, stderr=log, timeout=180,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    text = (out / "run.log").read_text()
    assert proc.returncode == 0 and "[input_script] EXIT" in text, (name, proc.returncode, text[-4000:])
    misses = tomllib.loads((out / "dispatch_misses.toml").read_text())
    assert not misses.get("functions", {}).get("extra"), (name, misses)
    results.append(name)
    print("PASS", name, flush=True)
    return out


def ram(out, name):
    blob = (out / (name + ".state")).read_bytes()
    sections = struct.unpack_from("<6I", blob, 88)
    at = 112 + sections[0] + sections[1]
    return blob[at:at+65536]


def word(r, at):
    return int.from_bytes(r[at:at+2], "big")


# Identical native Sonic action/animation results, with the party adapter
# active in BOTH runs. Only rendering/DMA metadata is excluded from comparison.
movement = start + capture("standing") + ["HOLD DOWN", "WAIT 8", "PRESS C 2", "WAIT 4"] + capture("charge") + [
    "RELEASE DOWN", "WAIT 16"] + capture("dash") + ["RELEASE", "WAIT 2",
    "LOAD_STATE standing.state", "WAIT 3", "PRESS C 12", "WAIT 4"] + capture("jump") + [
    "WAIT 12"] + capture("apex") + ["WAIT 60"] + capture("land") + [
    "HOLD RIGHT", "WAIT 30"] + capture("walk") + ["WAIT 75"] + capture("run") + ["RELEASE"]
stock = run("sonic-physics", ["sonic", "tails", "none", "none"], movement)
shadow = run(character + "-physics", [character, "tails", "none", "none"], movement)
fields = [(8, 0x18), (0x1A, 0x1F), (0x22, 0x2E), (0x30, 0x40)]
for name in ("standing", "charge", "dash", "jump", "apex", "land", "walk", "run"):
    expected, actual = ram(stock, name), ram(shadow, name)
    for lo, hi in fields:
        assert expected[0xB000+lo:0xB000+hi] == actual[0xB000+lo:0xB000+hi], (name, hex(lo),
            expected[0xB000+lo:0xB000+hi].hex(), actual[0xB000+lo:0xB000+hi].hex())
    assert expected[0xF760:0xF766] == actual[0xF760:0xF766], (name, "physics constants")
assert ram(shadow, "charge")[0xB01C] == 9, "Spindash did not charge"
assert ram(shadow, "jump")[0xB022] & 2, "Jump did not become airborne"
assert not ram(shadow, "land")[0xB022] & 2, "Jump did not land"
assert word(ram(shadow, "walk"), 0xB008) > word(ram(shadow, "standing"), 0xB008)
results.append("eight native Sonic physics/animation comparisons")

for player, roster in [(2, ["tails", character, "none", "none"]),
                        (3, ["sonic", "tails", character, character])]:
    out = run(f"{character}-p{player}", roster, start + capture("start") + [
        f"PLAYER {player}", "PRESS C 12", "WAIT 5"] + capture("jump") + [
        "HOLD RIGHT", "WAIT 90", "RELEASE"] + capture("move"), "16:9")
    r = ram(out, "jump")
    obj = 0xB040 if player == 2 else 0xCFC0
    assert r[obj] == 1 and r[obj+0x22] & 2, (player, "Shadow actor jump")
    if player == 3:
        assert r[0xCF80] == 1, "Second Shadow companion missing"

# Native two-player split-screen, including both competitors' presentation.
vs_start = start[:3] + ["PRESS DOWN 2", "WAIT 8", "PRESS START 2", "WAIT 220",
    "ASSERT_RAM8 FFF600 1C", "PRESS START 2", "WAIT 220", "ASSERT_RAM8 FFF600 0C"]
vs = run(character + "-vs", [character, "tails", "none", "none"], vs_start + capture("vs"))
assert word(ram(vs, "vs"), 0xFFD8), "Native VS mode not active"

# Seed eligibility only; native jump triggers transformation and ring drain.
super_lines = start + ["WRITE_RAM8 FFFFB1 7", "WRITE_RAM16 FFFE20 80", "HOLD B",
    "WAIT_RAM8 FFFE19 1", "RELEASE", "WAIT 90"] + capture("super") + [
    "WRITE_RAM16 FFFE20 1", "WRITE_RAM16 FFF670 0", "WAIT 65",
    "ASSERT_RAM8 FFFE19 0", "WAIT 50", "ASSERT_RAM8 FFF65F 0"] + capture("reverted")
powered = run(character + "-super", [character, "tails", "none", "none"], super_lines + [
    "WRITE_RAM32 FFB00C 10000000", "WRITE_RAM8 FFB022 2", "WRITE_RAM16 FFB012 100",
    "WAIT 550", "ASSERT_RAM8 FFF600 0C"] + capture("respawn"))
sr = ram(powered, "super")
assert sr[0xFE19] and 120 <= word(sr, 0xFE20) < 128
assert word(sr, 0xF760) == 0xA00 and word(ram(powered, "reverted"), 0xF760) == 0x600
respawn = ram(powered, "respawn")
assert respawn[0xB000] == 1 and respawn[0xB024] == 2 and not respawn[0xFE19]
assert respawn[0xFE12] == ram(powered, "reverted")[0xFE12] - 1, "Death did not spend a life"

# Select the fifth character through actual native title/Options input, then
# apply and boot gameplay. This also exercises persistent roster writing.
menu_lines = start[:3] + ["PRESS DOWN 2", "WAIT 8", "PRESS DOWN 2", "WAIT 8",
    "PRESS START 2", "WAIT 120", "ASSERT_RAM8 FFF600 24", "PRESS DOWN 2", "WAIT 8",
    "PRESS LEFT 2", "WAIT 12", "SCREENSHOT options-shadow.png", "PRESS B 2", "WAIT 340",
    "ASSERT_RAM8 FFF600 04", "WAIT_RAM8 FFB06F FF", "PRESS START 2", "WAIT 340",
    "ASSERT_RAM8 FFF600 0C"] + capture("selected")
menu = run(character + "-options", ["sonic", "tails", "none", "none"], menu_lines)
assert f"player1={character}\n" in (menu / "sonic2-party.ini").read_text(), "Options did not save Shadow"

(root / "results.json").write_text(json.dumps({"passed": results,
    "unverified": ["full campaign and every stage mechanic", "online peer simulation",
                   "subjective pose/art alignment beyond captured fixtures"],
    "fixtures": ["Super emerald/ring eligibility seeded; native transformation/drain/reversion executed"]}, indent=2) + "\n")
print(f"PASS {character} spike validation; inspect retained screenshots for visual QA", flush=True)
