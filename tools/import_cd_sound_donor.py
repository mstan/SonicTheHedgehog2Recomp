"""Import the native CD-DA event themes from an owned Sonic CD disc."""
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile
import wave

from import_sound_donor import atomic_write

def source_identity(disc):
    digest=hashlib.sha256()
    for lba in range(0,disc._sectors,128):digest.update(disc.sector(lba,min(128,disc._sectors-lba)))
    return digest.hexdigest()

TRACKS={'game-over':31,'act-clear':28,'invincibility':30}

def import_cd_donor(source, output, receipt, expected):
    from trilogy.scd_media import open_disc
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    with open_disc(source,output) as disc:
        header=disc.system_area()
        if (not header.startswith(b'SEGADISCSYSTEM') or header[0x120:0x150].strip(b' \0')!=b'SONIC THE HEDGEHOG-CD'):
            raise ValueError('Select an original Sonic CD disc image')
        region=header[0x1F0:0x200].strip(b' \0')
        donor='cd-us' if region==b'U' else None
        if donor not in expected:
            raise ValueError('The disc region does not match the selected Sonic CD soundtrack')
        identity=source_identity(disc)
        US_SHA256="db17110d5c9f80e7a0ec92ead5b23c9e524b53df9ff6b5069c7352e1d357925f"
        if identity!=US_SHA256:
            raise ValueError('Select the supported original US Sonic CD disc')
        bundle=output/(donor+'-'+identity)
        stage=Path(tempfile.mkdtemp(prefix='.cd-sound-',dir=output))
        try:
            lines=['format=1','donor='+donor,'source_sha256='+identity]
            for event,number in TRACKS.items():
                if not any(t.number==number and t.kind=='AUDIO' for t in disc.track_info):
                    raise ValueError('The selected image has no event audio tracks; choose the full CUE/BIN, SGCD or CD ZIP')
                raw=disc.audio(number)
                if not raw or len(raw)%4 or len(raw)>32*1024*1024:
                    raise ValueError('Missing or invalid Sonic CD event audio; a disc with audio tracks is required')
                target=stage/(event+'.wav')
                with wave.open(str(target),'wb') as audio:
                    audio.setnchannels(2);audio.setsampwidth(2);audio.setframerate(44100);audio.writeframes(raw)
                digest=hashlib.sha256(target.read_bytes()).hexdigest()
                lines.extend((event+'.path='+bundle.name+'/'+target.name,event+'.sha256='+digest))
            if bundle.exists():
                for event in TRACKS:
                    if (bundle/(event+'.wav')).read_bytes()!=(stage/(event+'.wav')).read_bytes():
                        raise ValueError('Cached CD audio changed; existing installation retained')
            else:
                os.rename(stage,bundle)
            manifest=output/(donor+'.ini')
            atomic_write(manifest,('\n'.join(lines)+'\n').encode())
            if receipt is not None:
                receipt=Path(receipt).resolve()
                if receipt.suffix.lower()!='.ini' or receipt==Path(source).resolve():
                    raise ValueError('Receipt must be separate INI metadata')
                atomic_write(receipt,f'format=1\ncontent={manifest}\n'.encode())
            return donor,manifest
        finally:
            if stage.exists():
                if stage.parent!=output or not stage.name.startswith('.cd-sound-'):
                    raise ValueError('Unsafe CD audio staging path')
                shutil.rmtree(stage)
