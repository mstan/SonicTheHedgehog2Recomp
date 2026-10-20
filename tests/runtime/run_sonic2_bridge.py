"""Owner-ROM EHZ bridge: all six actors land, ride sag, jump and leave.

Only player/camera approach positioning is bypassed. Native bridge handlers
must create standing contacts and the changing log heights themselves.
"""
import argparse,json,os,shutil,struct,subprocess,tomllib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('exe','rom','config','out'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--expect-failure',action='store_true')
a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
exe=out/a.exe.name;shutil.copy2(a.exe,exe);shutil.copy2(a.exe.parent/'SDL2.dll',out/'SDL2.dll')
shutil.copy2(a.config,out/'sonic2-party.ini')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software',GENESIS_STRICT_JSR_STACK='1')
actors=(0xB000,0xB040,0xCFC0,0xCF80,0xCF40,0xCF00)
def wr(bits,at,v):return f'WRITE_RAM{bits} {0xFF0000+at:X} {v&((1<<bits)-1):X}'
def capture(n):return [f'SCREENSHOT {n}.png',f'SAVE_STATE {n}.state','WAIT 8']
def pause():return ['PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 1']
def resume():return ['PRESS START 2','WAIT 12','ASSERT_RAM16 FFF63A 0']
def place(at,x,y):return [wr(32,at+8,x<<16),wr(32,at+12,y<<16),wr(32,at+0x10,0),
    wr(16,at+0x14,0),wr(8,at+0x22,2),wr(8,at+0x1c,0),wr(8,at+0x3c,0),
    wr(8,at+0x2B,2),wr(16,at+0x32,1200)] # bypass Buzzer damage, never platform contacts
lines=['WAIT 600','PRESS START 2','WAIT 40','PRESS START 2','WAIT 340','ASSERT_RAM8 FFF600 0C']
lines+=pause()+place(0xB000,0x510,0x230)+resume()+['WAIT 450']+capture('approach')
lines+=pause()
for i,at in enumerate(actors):lines+=place(at,0x5A8+(i-2)*20,0x230)
lines+=resume()+['WAIT 24']+capture('land')+['WAIT 20']+capture('ride')+[
    'PRESS C 2','WAIT 10']+capture('jump')+['WAIT 90']+capture('return')+[
    'HOLD RIGHT','WAIT 85','RELEASE']+capture('exit')+['EXIT']
(out/'bridge.input').write_text('\n'.join(lines)+'\n')
with (out/'run.log').open('w') as log:
 r=subprocess.run([str(exe),str(a.rom.resolve()),'--no-launcher','--input-script','bridge.input','--widescreen','16:9',
    '--target-fps','1000','--max-frames','6000'],cwd=out,env=env,stdout=log,stderr=log,timeout=180,
    creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
text=(out/'run.log').read_text();assert r.returncode==0 and '[input_script] EXIT' in text,(r.returncode,text[-3000:])
assert not tomllib.loads((out/'dispatch_misses.toml').read_text()).get('functions',{}).get('extra')
def ram(name):
 b=(out/(name+'.state')).read_bytes();s=struct.unpack_from('<6I',b,88);return b[112+s[0]+s[1]:][:65536]
def word(r,at):return int.from_bytes(r[at:at+2],'big')
observations={}
for n in ('approach','land','ride','jump','return','exit'):
 r=ram(n);observations[n]=[dict(player=i+1,x=word(r,at+8),y=word(r,at+12),status=r[at+0x22],routine=r[at+0x24]) for i,at in enumerate(actors)]
(out/'observations.json').write_text(json.dumps(observations,indent=2)+'\n')
land=ram('land');ride=ram('ride');back=ram('return')
standing=[bool(land[at+0x22]&8 or ride[at+0x22]&8 or back[at+0x22]&8) for at in actors]
print('Native standing contacts',standing,flush=True)
if a.expect_failure:
 assert standing[:2]==[True,True] and not any(standing[2:]),observations
 print('REPRODUCED extra actors fall through native EHZ bridge',flush=True)
else:
 assert all(standing),observations
 assert ram('jump')[0xB022]&2,'P1 did not jump off bridge'
 # Remove the native pair's weight through real movement off the bridge.
 # Extra standing contacts, restored with the state, must keep it depressed.
 parents=[o for o in range(0xB400,0xD000,64) if back[o]==0x11 and not back[o+1]&0x40 and word(back,o+8)==0x5A8]
 assert len(parents)==1,parents
 bridge=parents[0]
 extra=['WAIT 2','LOAD_STATE return.state','WAIT 2']+pause()+place(0xB000,0x510,0x230)+place(0xB040,0x4F0,0x230)+resume()+[
  'WAIT 8']+capture('extra-weight')+['WAIT 180']+capture('relaxed')+['EXIT']
 (out/'extra.input').write_text('\n'.join(extra)+'\n')
 with (out/'extra.log').open('w') as log:
  result=subprocess.run([str(exe),str(a.rom.resolve()),'--no-launcher','--input-script','extra.input','--widescreen','16:9',
   '--target-fps','1000','--max-frames','1500'],cwd=out,env=env,stdout=log,stderr=log,timeout=90,
   creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
 assert result.returncode==0,(out/'extra.log').read_text()[-3000:]
 weighted=ram('extra-weight');relaxed=ram('relaxed')
 assert not weighted[bridge+0x22]&0x18,'Native pair still supplies weight'
 assert weighted[bridge+0x3E]==64 and any(weighted[o+0x22]&8 for o in actors[2:]),'Extra-only weight did not retain native sag'
 assert not relaxed[bridge+0x3E] and all(not relaxed[o+0x22]&8 for o in actors),'Bridge did not relax after everyone left'
 assert not tomllib.loads((out/'dispatch_misses.toml').read_text()).get('functions',{}).get('extra')
 print('PASS all six stand, jump and cross; extra-only weight retains sag, leaving relaxes it',flush=True)
