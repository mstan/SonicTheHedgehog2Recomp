#!/usr/bin/env python3
"""Prepare private native-generation inputs from a verified sound-only probe."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib
from build import HEADER, FIELDS, SOURCE_COMMIT

sys.dont_write_bytecode = True

def write(path, data):
    if isinstance(data,str): data=data.encode("utf-8")
    if not path.exists() or path.read_bytes()!=data: path.write_bytes(data)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("probe", "game", "engine", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    probe, game, engine, out = (getattr(a, k).resolve() for k in ("probe", "game", "engine", "output"))
    if any(out.is_relative_to(x) or x.is_relative_to(out) for x in (probe,game,engine)):
        p.error("generation output must be separate from the game, engine and probe inputs")
    marker=out / ".sonic2-clone-generation"
    if out.exists() and not marker.is_file() and any(out.iterdir()):
        p.error("output must be new, empty or previously prepared by this tool")
    rom = (game / "sonic2.bin").read_bytes()
    if len(rom) != 0x100000 or zlib.crc32(rom) != 0x7B905383:
        p.error("native generation requires the original Sonic 2 JUE REV01 ROM")
    info = json.loads((probe / "probe.json").read_text())
    payload_file=probe / "clone-driver.bin"
    if not HEADER.size <= payload_file.stat().st_size <= 0x380000:
        p.error("sound payload exceeds the available cartridge bank")
    payload = payload_file.read_bytes()
    rom_assets=info.get("asset_mode")=="rom"
    if not rom_assets:
        p.error("full sound packs are no longer runtime inputs; use automatic preparation or a software-only --rom-assets build")
    digest = hashlib.sha256(payload).hexdigest()
    magic,*values=HEADER.unpack_from(payload)
    if (magic!=(b"S2CR" if rom_assets else b"S2CD") or any(info.get(k)!=v for k,v in zip(FIELDS,values)) or
            info["source_commit"] != SOURCE_COMMIT or
            digest != info["payload_sha256"] or len(payload) != info["payload_size"] or
            info["load_address"] != len(rom) or
            struct.unpack_from(">I", payload, 4)[0] != len(payload)):
        p.error("probe identity or image bounds mismatch")
    if not (info["ram_address"] & 0xFFFFFF == 0xFFF200 and 0 < info["ram_size"] <= 0x400 and
            0 <= info["queue_offset"] <= info["ram_size"]-8 and 0 <= info["pause_offset"] < info["ram_size"]):
        p.error("sound RAM bounds mismatch")
    image = rom + payload
    spec = importlib.util.spec_from_file_location("stock_disasm", engine / "tools/sonic_disassembly.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    labels = {pc: names for pc, names in module.symbols(probe / "driver.lst", image).items() if pc >= len(rom)}
    entries = {info[k] for k in ("initialise", "update", "queue_music", "queue_sfx", "queue_extra",
                                "megapcm_load_driver", "megapcm_load_table", "guarded_update")}
    entries.update(labels)
    out.mkdir(parents=True, exist_ok=True)
    write(marker,"Sonic 2 private clone driver generation inputs\n")
    for source in (*game.glob("*.toml"), *game.glob("*.csv")):
        if source.name not in ("game.toml","annotations_from_disasm.csv"):write(out / source.name,source.read_bytes())
    config = (game / "game.toml").read_text(encoding="utf-8")
    config, count = re.subn(r"discovery_files\s*=\s*\[", 'discovery_files = [\n    "sonic2.clone_driver.toml", "sonic2.clone_tables.toml",', config, count=1)
    if count != 1:
        p.error("game discovery list missing")
    write(out / "game.toml",config)
    write(out / "sonic2.bin",image)
    write(out / "sonic2.clone_driver.toml",
        "[functions]\nextra = [\n" + "".join(f"    0x{pc:06X},\n" for pc in sorted(entries)) + "]\n")
    write(out / "clone-code.txt","".join(f"{pc:06X}\n" for pc in sorted(module.instruction_starts(probe / "driver.lst", image))))
    subprocess.run([sys.executable, "-B", str(engine / "tests/tools/gen_disasm_jumptables.py"),
                    "--lst", str(probe / "driver.lst"), "--code-addrs", str(out / "clone-code.txt"),
                    "--output", str(out / "clone_tables.next"), "--prefix", "sonic2.clone"], check=True)
    write(out / "sonic2.clone_tables.toml",(out / "clone_tables.next").read_bytes())
    (out / "clone_tables.next").unlink()
    annotations=(game / "annotations_from_disasm.csv").read_text(encoding="utf-8")
    annotations+="".join(f"{pc:06X},Clone_{names[0]},\n" for pc,names in sorted(labels.items()))
    write(out / "annotations_from_disasm.csv",annotations)
    header = "/* Generated sound image identity; no game data. */\n#pragma once\n"
    header += f'#define S2_CLONE_SHA256 "{digest}"\n'
    for key in ("load_address", "payload_size", "initialise", "update", "queue_music", "queue_sfx", "queue_extra",
                "ram_address", "ram_size", "sample_table", "queue_offset", "pause_offset", "guarded_update"):
        header += f"#define S2_CLONE_{key.upper()} 0x{info[key]:X}u\n"
    if rom_assets:
        header+="#define S2_CLONE_ASSETS_FROM_ROM 1\n"
        data_labels={}
        for line in (probe/"driver.lst").read_text(errors="replace").splitlines():
            row=module.ROW.match(line)
            label=module.LABEL.match(row[2]) if row else None
            if label:data_labels[label[1]]=int(row[1],16)
        for name,macro in {"MusicIndex":"MUSICINDEX","SoundIndex":"SOUNDINDEX",
                           "PSG_Index":"PSG_INDEX",
                           "StopAllSound":"STOP_ALL","Clone_PCMIdle":"PCM_IDLE"}.items():
            address=data_labels[name]
            if not len(rom)<=address<len(image):p.error("driver table outside software image")
            header+=f"#define S2_CLONE_{macro} 0x{address:X}u\n"
        header+="#define S2_CLONE_SPECSOUNDINDEX S2_CLONE_SOUNDINDEX\n"
        program="/* Generated driver software only; no music, effects or sample data. */\n#include <stdint.h>\n"
        program+="static const uint8_t s2_clone_program[] = {\n"+"".join(
            "    "+",".join(f"0x{b:02X}" for b in payload[i:i+16])+",\n" for i in range(0,len(payload),16))+"};\n"
        write(out/"sonic2_clone_program.h",program)
    write(out / "sonic2_clone_metadata.h",header)
    print(f"Prepared {len(entries)} sound entry seeds; original {len(rom)} ROM bytes preserved; payload {digest}")


if __name__ == "__main__":
    main()
