"""Frozen entry point for the same bounded sound donor importer."""
from pathlib import Path
import runpy
import sys

root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(root / 'tools'))
name='import_custom_smps.py' if 'custom-smps' in sys.argv else 'import_sound_donor.py'
runpy.run_path(str(root / 'tools' / name), run_name='__main__')
