"""Build/verify the game-owned pinned Sonic 2 source; export source annotations.

Uses the engine's listing parser/exporter; game identity and source pin stay here.
Pass --engine for a development checkout, then normal --out/--reuse/--install.
ROMs/listings are private build artifacts. Never edits the source submodule.
"""
import argparse
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--engine', type=Path)
    known, rest = parser.parse_known_args()
    engine = known.engine or ROOT / ('engine-local' if (ROOT / 'engine-local').is_dir() else 'segagenesisrecomp')
    spec = importlib.util.spec_from_file_location('disassembly', engine / 'tools/sonic_disassembly.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = ROOT
    module.SOURCES = {'sonic2': ('game/s2disasm', '65ddcc24250af08ddfdf58e36351374ace998e66',
        'build.lua', 's2built.bin', 's2.lst', 'game/sonic2.bin')}
    module.FOLDERS = {'sonic2': 'game'}
    sys.argv = [sys.argv[0], *rest]
    module.main()
