"""Frozen entry point for the same bounded sound donor importer."""
from pathlib import Path
import runpy
import sys

root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(root / 'tools'))
runpy.run_path(str(root / 'tools/import_sound_donor.py'), run_name='__main__')
