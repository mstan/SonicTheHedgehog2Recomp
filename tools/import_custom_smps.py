"""Assemble one data-only Sonic 1 68K SMPS source into the private cache."""
import argparse
import re
from pathlib import Path
from assemble_custom_smps import assemble
from import_sound_donor import atomic_write

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign',choices=['custom-smps'])
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--expected',required=True)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--receipt',required=True,type=Path)
    args=parser.parse_args()
    try:
        if args.source.stat().st_size>1024*1024:raise ValueError('SMPS source exceeds 1 MiB')
        data=assemble(args.source.read_text(encoding='utf-8-sig'))
        args.out.mkdir(parents=True,exist_ok=True)
        if not re.fullmatch('[a-z0-9-]{1,63}',args.expected):raise ValueError('Invalid custom track')
        output=args.out/('assembled-'+args.expected+'.bin')
        if output.resolve()==args.source.resolve()or args.receipt.resolve()in(output.resolve(),args.source.resolve()):
            raise ValueError('Import output must be separate from the source')
        if output.is_symlink():raise ValueError('Import cache must not contain symbolic links')
        atomic_write(output,data)
        atomic_write(args.receipt,f'format=1\ncontent={output}\n'.encode('utf-8'))
        print('S3K_CD_PROGRESS:1:SMPS source assembled',flush=True)
    except (OSError,ValueError)as error:
        print('S3K_CD_ERROR:'+str(error),flush=True)
        return 1
    return 0

if __name__=='__main__':
    raise SystemExit(main())
