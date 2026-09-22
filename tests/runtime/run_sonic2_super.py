"""Serial owner-ROM Super fixtures: RAM prerequisites, real jump/lifecycle.

Only fixture RAM is seeded, never ROMs or the owner's campaign/save states.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import struct
import tomllib

p=argparse.ArgumentParser(description=__doc__)
for name in ('exe','rom','amy','s3k','out'):
    p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
root=a.out.resolve(); root.mkdir(parents=True,exist_ok=False)
shutil.copy2(a.exe,root/a.exe.name)
shutil.copy2(a.exe.parent/'SDL2.dll',root/'SDL2.dll')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software',GENESIS_STRICT_JSR_STACK='1')
results=[]
def configure(first,donors=True):
    roster=[first]+[c for c in ('sonic','tails','knuckles','amy') if c!=first]
    if not donors: roster=roster[:2]+['none','none']
    (root/'sonic2-party.ini').write_text(f'slots={4 if donors else 2}\namy_enabled={int(donors)}\ns3k_enabled={int(donors)}\nsave_menu_enabled=0\n'+
        f'amy_path={a.amy.resolve().as_posix()}\ns3k_path={a.s3k.resolve().as_posix()}\n'+
        ''.join(f'player{i+1}={c}\n' for i,c in enumerate(roster)))
    return roster
def run(name,lines):
    (root/(name+'.input')).write_text('\n'.join(lines+['EXIT'])+'\n')
    with (root/(name+'.log')).open('w') as log:
        result=subprocess.run([str(root/a.exe.name),str(a.rom.resolve()),'--no-launcher','--widescreen','off',
            '--input-script',name+'.input','--max-frames','8500','--target-fps','1000'],cwd=root,env=env,
            stdout=log,stderr=log,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    assert not tomllib.loads((root/'dispatch_misses.toml').read_text()).get('functions',{}).get('extra'),name
    text=(root/(name+'.log')).read_text()
    assert '[dispatch]' not in text,(name,'native dispatch failure',text[-3000:])
    assert result.returncode==0 and '[input_script] EXIT' in text,(name,result.returncode,text[-3000:])
    results.append(name)
    return text
def capture(name):
    # Frame scripts can observe temporary companion-owned Super/physics RAM.
    # Quickstates serialize only after those native adapter scopes have closed.
    return [f'SCREENSHOT {name}.png',f'SAVE_STATE {name}.state','WAIT 8']
def ram(name):
    blob=(root/(name+'.state')).read_bytes()
    sections=struct.unpack_from('<6I',blob,88)
    at=112+sections[0]+sections[1]
    return blob[at:at+65536]
def word(r,at): return int.from_bytes(r[at:at+2],'big')
def load(name): return ['WAIT 2',f'LOAD_STATE {name}.state','WAIT 1']
def object_at(at,identity,x,y):
    return [f'WRITE_RAM32 {0xff0000+at+i:X} 0' for i in range(0,64,4)]+[
        f'WRITE_RAM8 {0xff0000+at:X} {identity:X}',
        f'WRITE_RAM16 {0xff0008+at:X} {x:X}',f'WRITE_RAM16 {0xff000c+at:X} {y:X}']
start=['WAIT 600','PRESS START 2','WAIT 40','PRESS START 2','WAIT 220','ASSERT_RAM8 FFF600 0C']
emeralds=['WRITE_RAM8 FFFFB1 7']+[f'WRITE_RAM8 {0xffffb2+i:X} FF' for i in range(7)]
for first in ('sonic','tails','amy','knuckles'):
    roster=configure(first)
    name=first+'-lifecycle'
    run(name,start+capture(first+'-normal')+emeralds+['WRITE_RAM16 FFFE20 80',
        'HOLD B','WAIT_RAM8 FFFE19 1']+capture(first+'-transform')+[
        'RELEASE','WAIT 90']+
        capture(first+'-super')+['SAVE_STATE '+first+'.state','WAIT 8',
        'WRITE_RAM16 FFFE20 1','WRITE_RAM16 FFF670 0','WAIT 65',
        'ASSERT_RAM8 FFFE19 0','WAIT 50','ASSERT_RAM8 FFF65F 0','ASSERT_RAM8 FFB02A 0']+
        capture(first+'-reverted')+['WRITE_RAM16 FFFE20 80','HOLD B','WAIT_RAM8 FFFE19 1',
        'RELEASE','WAIT 90','ASSERT_RAM8 FFB02A 0']+capture(first+'-again'))
    normal=ram(first+'-normal'); powered=ram(first+'-super'); reverted=ram(first+'-reverted')
    assert powered[0xfe19]==1 and reverted[0xfe19]==0,first+' Super lifecycle'
    assert powered[0xb000]==(2 if first=='tails' else 1),first+' identity'
    assert 120<=word(powered,0xfe20)<128,first+' native ring drain'
    assert powered[0xb02b]&2 and not reverted[0xb02b]&2,first+' invincibility'
    physics=0xfec0 if first=='tails' else 0xf760
    assert word(powered,physics)==0xa00 and word(reverted,physics)==0x600,first+' physics'
    if first!='sonic':
        assert normal[0xfb04:0xfb0c]==powered[0xfb04:0xfb0c],first+' companion Sonic palette'
    assert word(reverted,0xfe20)==0,first+' depleted rings'
    assert not any(powered[o+0x2b]&2 for o in (0xb040,0xcfc0,0xcf80)),first+' companions stay normal'

    run(first+'-eligibility',load(first+'-normal')+emeralds+[
        'WRITE_RAM16 FFFE20 31','HOLD B','WAIT 85','RELEASE']+capture(first+'-49-rings')+[
        'WRITE_RAM16 FFFE20 32','WRITE_RAM8 FFFFB1 6','HOLD B','WAIT 85','RELEASE']+
        capture(first+'-6-emeralds')+['WRITE_RAM8 FFFFB1 7','WAIT 30']+
        capture(first+'-grounded')+['HOLD B','WAIT_RAM8 FFFE19 1','RELEASE','WAIT 80']+
        capture(first+'-50-rings'))
    for suffix in ('49-rings','6-emeralds','grounded'):
        assert ram(first+'-'+suffix)[0xfe19]==0,first+' '+suffix
    assert ram(first+'-50-rings')[0xfe19]==1,first+' exact 50-ring eligibility'

    for phase in ('transform','super'):
        state=first+'-'+phase
        branch=['HOLD RIGHT','WAIT 60','RELEASE']
        end=lambda name:[f'DUMP_RAM {name}.bin',f'SCREENSHOT {name}.png','WAIT 1']
        text=run(state+'-restore',load(state)+branch+end(state+'-branch1')+
            [f'LOAD_STATE {state}.state','WAIT 1']+branch+end(state+'-branch2'))
        assert text.count('[LOAD] loaded')==2,state
        run(state+'-cold',load(state)+branch+end(state+'-cold'))
        for extension in ('.bin','.png'):
            expected=(root/(state+'-branch1'+extension)).read_bytes()
            assert expected==(root/(state+'-branch2'+extension)).read_bytes(),state+' repeat '+extension
            assert expected==(root/(state+'-cold'+extension)).read_bytes(),state+' cold '+extension

    run(first+'-pause',load(first+'-super')+['PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 1']+
        capture(first+'-pause-before')+['WAIT 120']+capture(first+'-pause-after')+[
        'PRESS START 2','WAIT 70']+capture(first+'-unpaused'))
    assert word(ram(first+'-pause-before'),0xfe20)==word(ram(first+'-pause-after'),0xfe20),first+' pause drain'
    assert word(ram(first+'-unpaused'),0xfe20)<word(ram(first+'-pause-after'),0xfe20),first+' resume drain'

    # Ordinary enemy contact, without a jump/hammer attack, must not hurt Super.
    run(first+'-contact',load(first+'-super')+object_at(0xb900,0x4b,96,656)+[
        'WAIT 8']+capture(first+'-contact'))
    contact=ram(first+'-contact')
    assert contact[0xb024]==2 and contact[0xb02b]&2 and word(contact,0xb030)==0,first+' protected contact'
    assert int.from_bytes(contact[0xfe26:0xfe2a],'big')>0,first+' native enemy defeated'

    run(first+'-act-end',load(first+'-super')+['WRITE_RAM8 FFFE1E 0','WAIT 60']+
        capture(first+'-end')+object_at(0xb800,0x3a,0,0)+[
        'WRITE_RAM8 FFB824 10','WAIT 400','ASSERT_RAM16 FFFE10 1']+capture(first+'-next-act'))
    assert not ram(first+'-end')[0xfe19] and not ram(first+'-end')[0xf65f],first+' act-end revert'
    assert not ram(first+'-next-act')[0xfe19],first+' next act'
    assert ram(first+'-next-act')[0xb000]==(2 if first=='tails' else 1),first+' next-act identity'

    # Place P1 beyond the death boundary and let native KillCharacter set the
    # complete death state, instead of partially constructing routine 6.
    bottom=word(powered,0xeece)+0x200
    run(first+'-death',load(first+'-super')+[f'WRITE_RAM32 FFB00C {bottom<<16:X}',
        'WRITE_RAM8 FFB022 2','WRITE_RAM16 FFB012 100','WAIT 500','ASSERT_RAM8 FFF600 0C']+
        capture(first+'-respawn'))
    respawn=ram(first+'-respawn')
    assert not respawn[0xfe19] and not respawn[0xf65f] and respawn[0xb02a]==0,first+' death clears Super'
    assert respawn[0xb000]==(2 if first=='tails' else 1) and respawn[0xb024]==2,first+' respawn identity'

    # Load a real water zone before placing P1 across its actual water plane.
    # Enabling water in EHZ is invalid: its zone-specific water dispatch is data.
    run(first+'-arz',load(first+'-normal')+['WRITE_RAM16 FFFE10 F00',
        'WRITE_RAM16 FFFE02 1','WAIT 400','ASSERT_RAM16 FFFE10 F00']+emeralds+[
        'WRITE_RAM16 FFFE20 80','HOLD B','WAIT_RAM8 FFFE19 1','RELEASE','WAIT 90']+
        capture(first+'-arz'))
    water_ram=ram(first+'-arz'); plane=word(water_ram,0xf646)
    # Pause around placement: a VBlank can otherwise interrupt an imported
    # controller holding a local motion copy that overwrites injected RAM.
    pause=['PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 1']
    resume=['PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 0']
    enter=pause+[f'WRITE_RAM32 FFB00C {(plane+48)<<16:X}',
        'WRITE_RAM8 FFB022 2','WRITE_RAM32 FFB010 0','WRITE_RAM16 FFB014 0']+resume
    leave=pause+[f'WRITE_RAM32 FFB00C {word(water_ram,0xb00c)<<16:X}',
        'WRITE_RAM32 FFB010 0','WRITE_RAM16 FFB014 0']+resume
    run(first+'-water',load(first+'-arz')+enter+capture(first+'-underwater')+
        leave+capture(first+'-out-water')+enter+['WRITE_RAM16 FFFE20 1',
        'WRITE_RAM16 FFF670 0','WAIT 60']+capture(first+'-water-revert'))
    assert word(ram(first+'-underwater'),physics)==0x500,first+' underwater Super physics'
    assert word(ram(first+'-out-water'),physics)==0xa00,first+' water exit Super physics'
    assert word(ram(first+'-water-revert'),physics)==0x300,first+' water revert physics'

    run(first+'-special',load(first+'-super')+['WRITE_RAM8 FFF600 10','WAIT 500',
        'ASSERT_RAM8 FFF600 10']+capture(first+'-halfpipe')+[
        'WAIT_RAM8 FFF600 0C','WAIT 160']+capture(first+'-special-return'))
    returned=ram(first+'-special-return')
    assert not returned[0xfe19] and not returned[0xf65f],first+' special-return revert'
    assert returned[0xb000]==(2 if first=='tails' else 1),first+' special-return identity'

    vs_start=['WAIT 600','PRESS START 2','WAIT 40','PRESS DOWN 2','WAIT 8',
        'PRESS START 2','WAIT 220','ASSERT_RAM8 FFF600 1C','PRESS START 2',
        'WAIT 220','ASSERT_RAM8 FFF600 0C','ASSERT_RAM16 FFFFD8 1']
    run(first+'-versus',vs_start+emeralds+['WRITE_RAM16 FFFE20 80','HOLD B','WAIT 85','RELEASE']+
        capture(first+'-versus'))
    assert not ram(first+'-versus')[0xfe19],first+' no Super in VS'

    if first=='amy':
        run('amy-hammer',load('amy-super')+['PRESS A 2','WAIT 12']+capture('amy-hammer'))
        assert ram('amy-hammer')[0xb01c] in (0x23,0x28),'Super Amy hammer'
    if first=='knuckles':
        run('knuckles-glide',load('knuckles-super')+[
            'HOLD B','WAIT 24','RELEASE','WAIT 2','HOLD B','WAIT 8']+capture('knuckles-glide'))
        assert ram('knuckles-glide')[0xb01c]==0x20,'Super Knuckles glide'

for first in ('sonic','tails'):
    configure(first,False)
    run(first+'-no-donors',start+emeralds+['WRITE_RAM16 FFFE20 80','HOLD B',
        'WAIT_RAM8 FFFE19 1','RELEASE','WAIT 90']+capture(first+'-no-donors'))
    assert ram(first+'-no-donors')[0xfe19]==1,first+' works without donor mods'
print(f'PASS {len(results)} Super fixtures')
