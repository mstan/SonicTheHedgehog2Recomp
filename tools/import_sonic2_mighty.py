"""Verify and privately stage the exact Megamix v3 ROM for Mighty and Shadow.

Accepts the same raw cartridge or single-ROM ZIP as the Sonic 3 importer.
No stages or executable donor code are imported into Sonic 2.
"""
import argparse
import hashlib
from pathlib import Path
import zipfile

SIZE = 1283224
SHA256 = "85d9e9ba79f0035d5854618666ff198871abe1931558496e6563b1cd90e5cdbe"

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("donor", type=Path)
p.add_argument("--runtime", type=Path, required=True)
a = p.parse_args()
if zipfile.is_zipfile(a.donor):
    with zipfile.ZipFile(a.donor) as z:
        entries = [i for i in z.infolist() if not i.is_dir() and i.filename.lower().endswith((".bin", ".md", ".gen"))]
        if len(entries) != 1 or entries[0].file_size != SIZE:
            raise SystemExit("Expected one exact Megamix v3 cartridge")
        data = z.read(entries[0])
else:
    if a.donor.stat().st_size != SIZE:
        raise SystemExit("Wrong Megamix v3 cartridge size")
    data = a.donor.read_bytes()
if len(data) != SIZE or hashlib.sha256(data).hexdigest() != SHA256:
    raise SystemExit("Wrong Megamix revision (SHA-256 mismatch)")
target = a.runtime / "assets/characters/megamix-v3.bin"
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(data)
print(f"Verified private Mighty donor staged: {target.resolve()}")
