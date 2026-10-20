"""Muted owner-ROM six-character party, cold quickstate and loop fixtures.

Teleports bypass route traversal only. Native input owns movement, loop
attachment, death and retry; no success flags are set.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tomllib

p=argparse.ArgumentParser(description=__doc__)
for name in ('exe','rom','donor','amy','s3k','out'):
    p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software',GENESIS_STRICT_JSR_STACK='1')
roster=('shadow','tails','sonic','knuckles','amy','mighty')
settings='slots=6\namy_enabled=1\ns3k_enabled=1\nshadow_enabled=1\nmighty_enabled=1\nsave_menu_enabled=0\n'
settings+=f'amy_path={a.amy.resolve().as_posix()}\ns3k_path={a.s3k.resolve().as_posix()}\nmighty_path={a.donor.resolve().as_posix()}\n'
settings+=''.join(f'player{i+1}={c}\n' for i,c in enumerate(roster))
start=['WAIT 600','PRESS START 2','WAIT 40','PRESS START 2','WAIT 340','ASSERT_RAM8 FFF600 0C']
actors=(0xB000,0xB040,0xCFC0,0xCF80,0xCF40,0xCF00)

def run(name,lines,wide='16:9',reuse=None):
    out=reuse or root/name
    out.mkdir(exist_ok=True)
    exe=out/a.exe.name
    if not reuse:
        shutil.copy2(a.exe,exe);shutil.copy2(a.exe.parent/'SDL2.dll',out/'SDL2.dll')
        (out/'sonic2-party.ini').write_text(settings)
    (out/(name+'.input')).write_text('\n'.join(lines+['EXIT'])+'\n')
    with (out/(name+'.log')).open('w') as log:
        r=subprocess.run([str(exe),str(a.rom.resolve()),'--no-launcher','--input-script',name+'.input',
            '--widescreen',wide,'--max-frames','9000','--target-fps','1000'],cwd=out,env=env,stdout=log,stderr=log,
            timeout=180,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    text=(out/(name+'.log')).read_text()
    assert r.returncode==0 and '[input_script] EXIT' in text,(name,r.returncode,text[-3000:])
    assert not tomllib.loads((out/'dispatch_misses.toml').read_text()).get('functions',{}).get('extra'),name
    print('RUN',name,'passed',flush=True)
    return out

def capture(name):return [f'SCREENSHOT {name}.png',f'SAVE_STATE {name}.state','WAIT 8']
def ram(out,name):
    b=(out/(name+'.state')).read_bytes();s=struct.unpack_from('<6I',b,88)
    return b[112+s[0]+s[1]:][:65536]
def word(r,at):return int.from_bytes(r[at:at+2],'big')

branch=['WAIT 1','HOLD RIGHT','WAIT 18','PRESS C 2','WAIT 12','RELEASE', 'WAIT 25']
out=run('party',start+capture('spawn')+['HOLD RIGHT','WAIT 85','RELEASE','WAIT 60']+capture('follow')+[
    'PRESS C 2','WAIT 14','PRESS C 2','WAIT 2']+capture('double')+[
    'LOAD_STATE double.state',*branch,'DUMP_RAM branch1.bin','WAIT 1',
    'LOAD_STATE double.state',*branch,'DUMP_RAM branch2.bin','WAIT 1',
    'LOAD_STATE follow.state','WAIT 1','WRITE_RAM32 FFB00C 10000000','WRITE_RAM8 FFB022 2',
    'WRITE_RAM16 FFB012 100','WAIT 550','ASSERT_RAM8 FFF600 0C']+capture('retry'))
for name in ('spawn','follow','double','retry'):
    r=ram(out,name)
    assert [r[o] for o in actors]==[1,2,1,1,1,1],(name,'six live objects')
    assert all(r[o+0x24]==2 for o in actors),(name,'actor routines')
assert word(ram(out,'follow'),0xB008)>word(ram(out,'spawn'),0xB008)+100,'Leader did not move'
assert all(word(ram(out,'follow'),o+8)>word(ram(out,'spawn'),o+8)+40 for o in actors[1:]),'Companion did not follow'
assert ram(out,'retry')[0xFE12]==ram(out,'follow')[0xFE12]-1,'Native death/retry did not spend one life'
assert (out/'branch1.bin').read_bytes()==(out/'branch2.bin').read_bytes(),'Six-actor double-jump continuation diverged'
run('cold',['WAIT 2','LOAD_STATE double.state',*branch,'DUMP_RAM cold.bin','WAIT 1'],reuse=out)
assert (out/'branch1.bin').read_bytes()==(out/'cold.bin').read_bytes(),'Fresh-process six-actor continuation diverged'

# EHZ2 loop: wait for the real camera, then let native terrain change the
# player's angle through the loop. Only the entry position/speed is seeded.
lines=start+['WRITE_RAM16 FFFE10 1','WRITE_RAM16 FFFE02 1','WAIT 400',
    'WRITE_RAM32 FFB008 10400000','WRITE_RAM32 FFB00C 3C00000','WRITE_RAM32 FFB010 0',
    'WRITE_RAM16 FFB014 0','WRITE_RAM8 FFB022 2','WRITE_RAM8 FFB01C 0','WAIT 800',
    'PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 1',
    'WRITE_RAM16 FFB010 C00','WRITE_RAM16 FFB014 C00',
    'HOLD RIGHT','PRESS START 2','WAIT 4','ASSERT_RAM16 FFF63A 0']
for i in range(90):lines+=['WAIT 2',f'SCREENSHOT pose-{i:03}.png',f'DUMP_RAM pose-{i:03}.bin']
loop=run('loop',lines,'off')
poses=[]
for i in range(90):
    r=(loop/f'pose-{i:03}.bin').read_bytes()
    poses.append(dict(index=i,x=word(r,0xB008),y=word(r,0xB00C),angle=r[0xB026],frame=r[0xB01A],status=r[0xB022],
        camera_x=word(r,0xEE00),camera_y=word(r,0xEE04)))
inverted=[d for d in poses if 0x60<=d['angle']<=0xA0 and not d['status']&2]
assert inverted and all(0<d['frame']<176 for d in inverted),'No real inverted donor frames'
assert all(0<=d['x']-d['camera_x']<320 and 0<=d['y']-d['camera_y']<224 for d in inverted),'Inverted captures off screen'
(loop/'poses.json').write_text(json.dumps(poses,indent=2)+'\n')
(root/'results.json').write_text(json.dumps(dict(passed=['six live actors follow and render','double-jump save/replay','cold continuation',
    'native death/retry rebuilds six actors','native loop includes inverted donor poses'],
    fixtures=['entry positions, speed and stage warp bypass traversal'],unverified=['six human controllers','six-peer netplay','full campaign']),indent=2)+'\n')
print('PASS six-character party and loop',flush=True)
