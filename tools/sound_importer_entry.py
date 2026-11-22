"""Frozen entry point for the same bounded sound donor importer."""
from pathlib import Path
import runpy
import sys

root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(root / 'tools'))
campaigns={'custom-smps':'import_custom_smps.py','custom-vgm':'import_custom_vgm.py'}
campaign=next((name for flag,name in zip(sys.argv[1:],sys.argv[2:]) if flag=='--campaign'),'')
name=campaigns.get(campaign,'import_sound_donor.py')
runpy.run_path(str(root / 'tools' / name), run_name='__main__')
