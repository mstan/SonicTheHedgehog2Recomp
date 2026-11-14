"""Freeze the sound-only ROM/CD importer; build hosts need PyInstaller."""
import argparse
import ast
import importlib.metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--work',required=True,type=Path)
    parser.add_argument('--engine-root',required=True,type=Path)
    args=parser.parse_args()
    out=args.out.resolve();work=args.work.resolve();engine=args.engine_root.resolve()
    out.mkdir(parents=True,exist_ok=True);work.mkdir(parents=True,exist_ok=True)
    command=[sys.executable,'-m','PyInstaller','--noconfirm','--onefile','--console',
             '--name','SonicSoundImporter','--distpath',str(out),'--workpath',str(work/'work'),
             '--specpath',str(work),'--exclude-module','tkinter','--exclude-module','numpy',
             '--exclude-module','matplotlib']
    separator=';'if sys.platform=='win32'else':'
    sources=[]
    for name in ['import_sound_donor.py','import_custom_smps.py','assemble_custom_smps.py','import_cd_sound_donor.py','trilogy/scd_media.py','trilogy/scd_iso.py']:
        source=ROOT/'tools'/name;sources.append(source)
        command+=['--add-data',str(source)+separator+str(Path('tools')/Path(name).parent)]
    for name in ['owned_media.py','disc_image.py','sgcd_owned_extract.py']:
        source=engine/'tools/segacd'/name;sources.append(source)
        command+=['--add-data',str(source)+separator+'tools/segacd']
    hidden=set()
    for source in sources:
        for node in ast.walk(ast.parse(source.read_text(encoding='utf-8'))):
            modules=[item.name for item in node.names]if isinstance(node,ast.Import)else[node.module]if isinstance(node,ast.ImportFrom)and not node.level and node.module else[]
            hidden.update(module for module in modules if module.split('.')[0]in sys.stdlib_module_names and module.split('.')[0]!='tkinter')
    for module in sorted(hidden):command+=['--hidden-import',module]
    command.append(str(ROOT/'tools/sound_importer_entry.py'))
    subprocess.run(command,check=True)
    licenses=out/'docs/sound-importer-licenses';licenses.mkdir(parents=True,exist_ok=True)
    candidates=[Path(sys.base_prefix)/'LICENSE.txt',Path(sys.base_prefix)/'LICENSE',
                Path(sys.base_prefix)/f'lib/python{sys.version_info.major}.{sys.version_info.minor}/LICENSE.txt',
                Path(f'/usr/share/doc/python{sys.version_info.major}.{sys.version_info.minor}/copyright')]
    license=next((path for path in candidates if path.is_file()),None)
    if license is None:raise RuntimeError('Python license was not found')
    shutil.copy2(license,licenses/'Python.txt')
    package=importlib.metadata.distribution('PyInstaller')
    for item in package.files or ():
        if item.name.lower().startswith(('license','copying'))and package.locate_file(item).is_file():
            shutil.copy2(package.locate_file(item),licenses/('PyInstaller-'+item.name))
    (licenses/'versions.json').write_text(json.dumps({'Python':sys.version.split()[0],'PyInstaller':package.version},indent=2)+'\n')

if __name__=='__main__':main()
