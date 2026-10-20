"""Muted Megamix Shadow mechanics fixtures, with native world outcomes.

Positions, enemy/monitor placement and Super eligibility are explicit cheats.
No target destruction, damage, ring-cost, transformation or success flags are
written. Retain screenshots, complete quickstates, logs and dispatch evidence.
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
for name in ("exe", "rom", "donor", "out"):
    p.add_argument("--"+name, type=Path, required=True)
p.add_argument("--only", choices=("all", "actions", "homing", "super", "blast", "presentation", "party"), default="all")
a = p.parse_args()
root = a.out.resolve()
root.mkdir(parents=True, exist_ok=False)
env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
           SDL_RENDER_DRIVER="software", GENESIS_STRICT_JSR_STACK="1")
start = ["WAIT 600", "PRESS START 2", "WAIT 40", "PRESS START 2", "WAIT 340", "ASSERT_RAM8 FFF600 0C"]
passed = []


def capture(name):
    return [f"SCREENSHOT {name}.png", f"SAVE_STATE {name}.state", "WAIT 8"]


def write(bits, at, value):
    return f"WRITE_RAM{bits} {0xFF0000+at:X} {value & ((1<<bits)-1):X}"


def obj(at, identity, x, y, subtype=0):
    return [f"ASSERT_RAM8 {0xFF0000+at:X} 0"] + [write(32, at+i, 0) for i in range(0, 64, 4)] + [
        write(8, at, identity), write(16, at+8, x), write(16, at+12, y), write(8, at+0x28, subtype)]


def run(name, lines, roster=("shadow", "tails", "none", "none")):
    out = root / name
    out.mkdir()
    exe = out / a.exe.name
    shutil.copy2(a.exe, exe)
    shutil.copy2(a.exe.parent / "SDL2.dll", out / "SDL2.dll")
    (out / "sonic2-party.ini").write_text("slots=4\namy_enabled=0\ns3k_enabled=0\nshadow_enabled=1\nmighty_enabled=0\n"+
        f"mighty_path={a.donor.resolve().as_posix()}\n"+
        "".join(f"player{i+1}={c}\n" for i,c in enumerate(roster)))
    (out / "check.input").write_text("\n".join(lines+["EXIT"])+"\n")
    with (out / "run.log").open("w") as log:
        result = subprocess.run([str(exe), str(a.rom.resolve()), "--no-launcher", "--input-script", "check.input",
            "--widescreen", "off", "--target-fps", "1000", "--max-frames", "9000"], cwd=out, env=env,
            stdout=log, stderr=log, timeout=180,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    log = (out / "run.log").read_text()
    misses = tomllib.loads((out / "dispatch_misses.toml").read_text()).get("functions",{}).get("extra")
    assert not misses, (name, misses)
    assert result.returncode == 0 and "[input_script] EXIT" in log, (name,result.returncode,log[-4000:])
    print("RUN", name, "passed", flush=True)
    return out


def ram(out, name):
    b = (out / (name+".state")).read_bytes()
    sizes = struct.unpack_from("<6I",b,88)
    at = 112+sizes[0]+sizes[1]
    return b[at:at+65536]


def word(r, at):
    return int.from_bytes(r[at:at+2],"big")


def signed(r, at):
    return int.from_bytes(r[at:at+2],"big",signed=True)


def load(name):
    return ["RELEASE",f"LOAD_STATE {name}.state","WAIT 2"]


if a.only in ("all","actions"):
    lines = start+capture("normal")+["PRESS C 2","WAIT 14"]+capture("jump")+[
        "PRESS C 2","WAIT 2"]+capture("double")+["WAIT 10","PRESS C 2","WAIT 2"]+capture("used")+[
        "WAIT 120"]+capture("land")+load("normal")+[write(16,0xFE20,16),"PRESS A 2","WAIT 2"]+capture("boost")+[
        *load("normal"),write(16,0xFE20,4),"PRESS A 2","WAIT 2"]+capture("poor")+load("normal")+[
        "HOLD DOWN","WAIT 8","PRESS C 2","WAIT 4"]+capture("charge")+[
        "RELEASE DOWN","WAIT 14"]+capture("dash")
    out = run("actions",lines)
    normal,jump,double,used,land,boost,poor,charge,dash = [ram(out,n) for n in
        ("normal","jump","double","used","land","boost","poor","charge","dash")]
    assert jump[0xB022]&2 and signed(double,0xB012)<-0x500, "Double jump did not relaunch"
    assert signed(used,0xB012)>signed(double,0xB012), "Repeated double jump"
    assert not land[0xB022]&2 and land[0xB016]==19, "Landing/reset"
    assert word(boost,0xFE20)==11 and abs(signed(boost,0xB014))>=0xB00, "Five-ring boost"
    assert word(poor,0xFE20)==4 and not poor[0xB022]&2 and not word(poor,0xB014), "Boost prerequisites / A is not jump"
    assert charge[0xB01C]==9 and abs(signed(dash,0xB010))>0x600, "Native spin dash"
    passed += ["native jump/landing", "one double jump per airtime", "five-ring ground boost", "insufficient-ring rejection", "spin dash"]

if a.only in ("all","homing"):
    for chain in (False,True):
        lines = start+[write(16,0xFE20,20)]+obj(0xB900,0x4B,176,600)
        if chain:
            lines += obj(0xB940,0x4B,256,600)
        lines += ["WAIT 4","PRESS C 2","WAIT 14"]+capture("jump")
        lines += (["HOLD A"] if chain else [])+["PRESS C 2","WAIT 2"]+capture("attack")
        if chain:
            lines += ["WAIT_RAM16 FFFE20 F"]+capture("paid")
        lines += ["WAIT 25","RELEASE"]+capture("hit")
        out = run("chain" if chain else "homing",lines)
        attack,hit = ram(out,"attack"),ram(out,"hit")
        assert abs(signed(attack,0xB010))>0x800, "Homing velocity"
        assert hit[0xB900] in (0,0x27) and int.from_bytes(hit[0xFE26:0xFE2A],"big")>0, "Real native enemy defeat"
        assert hit[0xB024]==2, "Homing hurt Shadow"
        if chain:
            assert hit[0xB940] in (0,0x27), "Second native enemy not defeated"
            assert word(ram(out,"paid"),0xFE20)==15, "Additional target must cost five rings"
        else:
            assert word(attack,0xFE20)==word(ram(out,"jump"),0xFE20), "First homing attack must be free"
        passed += ["paid multi-target chain" if chain else "native homing hit"]

if a.only in ("all","super"):
    lines = start+capture("normal")+[write(8,0xFFB1,7),write(16,0xFE20,128),
        "PRESS C 2","WAIT 8","HOLD B","PRESS C 2","WAIT_RAM8 FFFE19 1","RELEASE","WAIT 90"]+capture("super")+[
        "HOLD A","WAIT 3"]+capture("chaos")+["WAIT 45"]+capture("drain")+[
        "RELEASE A","WAIT 2"]+capture("released")+["PRESS C 2","WAIT 8","HOLD B","PRESS C 2","WAIT 2",
        "RELEASE","ASSERT_RAM8 FFFE19 0","WAIT 55"]+capture("reverted")
    out = run("super",lines)
    sr,chaos,drain,released,reverted = [ram(out,n) for n in ("super","chaos","drain","released","reverted")]
    assert sr[0xFE19] and not sr[0xB02A], "Native transform/control release"
    assert word(chaos,0xF760)==0x1100 and abs(signed(chaos,0xB014))>=0xC00, "Chaos Control launch"
    assert word(chaos,0xFE20)-word(drain,0xFE20)>=4, "Accelerated ring drain"
    assert word(released,0xF760)==0xC00, "Chaos Control release"
    assert not reverted[0xFE19] and not reverted[0xF65F] and word(reverted,0xF760)==0x600, "BC reversion"
    passed += ["BC native Super transform/revert", "Chaos Control speed and drain", "release restores Super physics"]

if a.only in ("all","presentation"):
    lines = start+capture("idle")+["WAIT 1700"]+capture("long-idle")+load("idle")+[
        write(32,0xB008,128<<16),write(32,0xB00C,592<<16),write(16,0xB010,0x300),write(16,0xB012,0x100),
        write(16,0xB014,0x300),write(8,0xB022,2),write(8,0xB01C,0),write(8,0xB03C,0),"WAIT 3"]+capture("free-air")+[
        "PRESS A 2","WAIT 1"]+capture("roll-in")
    out = run("presentation",lines)
    idle,air,rolled = [ram(out,n) for n in ("long-idle","free-air","roll-in")]
    assert 1<=idle[0xB01A]<=4, "Idle did not use donor idle script"
    assert air[0xB022]&6==2 and air[0xB01A]>=0xA9, "Free-fall donor pose"
    assert rolled[0xB022]&6==6 and rolled[0xB01C]==2, "Free-fall A roll-in"
    passed += ["long donor idle", "airborne pose has no skating effect", "A roll-in"]

if a.only in ("all","blast"):
    lines = start+[write(8,0xFFB1,7),write(16,0xFE20,128),"PRESS C 2","WAIT 8",
        "HOLD B","PRESS C 2","WAIT_RAM8 FFFE19 1","RELEASE","WAIT 90"]+capture("super")
    # A real native invincibility-monitor break and reward, then the ability.
    lines += obj(0xB900,0x26,128,640,7)+["WAIT 4","PRESS C 2","WAIT 8",
        write(32,0xB008,128<<16),write(32,0xB00C,592<<16),write(16,0xB010,0),write(16,0xB012,0x300),
        "WAIT 100"]+capture("monitor")
    lines += obj(0xB940,0x4B,208,640)+obj(0xB980,0x4B,256,640)+[
        "WAIT 4","PRESS C 2","WAIT 8","PRESS A 2","WAIT 2"]+capture("blast")
    out = run("blast",lines)
    reward,blast = ram(out,"monitor"),ram(out,"blast")
    assert word(reward,0xFEF4)>word(ram(out,"super"),0xFEF4) and reward[0xFE19], "Real monitor reward not acquired"
    assert blast[0xB940] in (0,0x27) and blast[0xB980] in (0,0x27), "Native Chaos Blast kills"
    assert blast[0xB024]==2 and blast[0xFE19], "Chaos Blast hurt/reverted Shadow"
    assert word(reward,0xFE20)-word(blast,0xFE20)<=1, "Chaos Blast charged rings"
    passed += ["real invincibility-monitor prerequisite", "Chaos Blast native enemy kills without extra ring cost"]

if a.only in ("all","party"):
    for player,roster in [(2,("tails","shadow","none","none")),(3,("sonic","tails","shadow","shadow"))]:
        lines=start+[f"PLAYER {player}","PRESS C 2","WAIT 14","PRESS C 2","WAIT 2"]+capture("double")
        out=run(f"p{player}",lines,roster)
        r=ram(out,"double");at=0xB040 if player==2 else 0xCFC0
        assert r[at]==1 and r[at+0x22]&2 and signed(r,at+0x12)<-0x500, "Companion double jump"
    vs=start[:3]+["PRESS DOWN 2","WAIT 8","PRESS START 2","WAIT 220","ASSERT_RAM8 FFF600 1C",
        "PRESS START 2","WAIT 340","ASSERT_RAM8 FFF600 0C"]+capture("vs")
    out=run("vs",vs)
    assert word(ram(out,"vs"),0xFFD8), "Native split-screen mode"
    passed += ["P2/P3 double jump and repeated Shadow companion", "native VS rendering"]

(root / "results.json").write_text(json.dumps({"passed":passed,
    "fixtures":["RAM positioning, enemy placement and Super eligibility only"],
    "unverified":["every stage and alternate route", "online peer simulation"]},indent=2)+"\n")
print("PASS",len(passed),"Shadow mechanics checks",flush=True)
