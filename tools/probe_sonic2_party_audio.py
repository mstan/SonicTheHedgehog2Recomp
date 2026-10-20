"""Muted, realtime party audio probe: WAV, delivery rings and host phases.

Run one case at a time. The SDL dummy device exercises callback delivery
without sending audio to the owner's speakers. This does not certify the
physical device; generated WAV and delivery evidence are reported separately.
"""
import argparse,json,os,shutil,socket,struct,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('exe','rom','config','out'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=('solo','two','six'),required=True)
p.add_argument('--port',type=int,default=4896)
p.add_argument('--device',choices=('dummy','wasapi'),default='dummy')
p.add_argument('--video',choices=('dummy','hidden'),default='dummy',help='Hidden uses the real Windows renderer and vsync')
p.add_argument('--cycles',type=int,default=1,help='Number of repeated running/jumping cycles')
p.add_argument('--light',action='store_true',help='Read bridge counters only; omit TCP, WAV and register logs')
p.add_argument('--stall-ms',type=int,default=0,help='Inject one producer-only stall after frame 1400 in this tool-owned child')
p.add_argument('--observer',type=Path,help='Native host-stall observer executable')
p.add_argument('--stack-snapshots',action='store_true',help='Copy/unwind native stacks during stalls in this tool-owned child only')
a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
exe=out/a.exe.name;shutil.copy2(a.exe,exe);shutil.copy2(a.exe.parent/'SDL2.dll',out/'SDL2.dll')
shutil.copy2(a.exe.with_suffix('.map'),exe.with_suffix('.map'))
config=a.config.read_text()
if a.case!='six':
 config=config.replace('slots=6',f'slots={1 if a.case=="solo" else 2}')
 for i in range(2 if a.case=='solo' else 3,7):
  config='\n'.join(f'player{i}=none' if x.startswith(f'player{i}=') else x for x in config.splitlines())+'\n'
(out/'sonic2-party.ini').write_text(config)
if not a.light:(out/'debug.ini').write_text(f'port={a.port}\n')
(out/'settings.ini').write_text('[audio]\nvolume=0\n[video]\nfullscreen=0\n')
lines=['WAIT 600','PRESS START 2','WAIT 40','PRESS START 2','WAIT 340','ASSERT_RAM8 FFF600 0C',
 'WAIT 120','HOLD RIGHT','WAIT 100','RELEASE','WAIT 80']
for cycle in range(a.cycles):
 for i in range(8):lines+=['PRESS C 8','WAIT 70']
lines+=['WAIT 240','EXIT']
(out/'probe.input').write_text('\n'.join(lines)+'\n')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER=a.device,SDL_RENDER_DRIVER='software',GENESIS_STRICT_JSR_STACK='1',GENESIS_HOST_TIMING_DUMP='1',
 GENESIS_AUDIO_WRITEDUMP=str(out/'chip-writes.txt'))
if a.light:env.pop('GENESIS_AUDIO_WRITEDUMP',None)
if a.video=='hidden':
 env['SDL_VIDEODRIVER']='windows';env.pop('SDL_RENDER_DRIVER',None)
 env.update(LNG_SCRIPT=str(out/'probe.input'),LNG_TEST_HIDDEN='1')
samples=[];seq=0
def rpc(sock,cmd,**kw):
 global seq
 seq+=1;sock.sendall((json.dumps(dict(id=seq,cmd=cmd,**kw))+'\n').encode())
 b=b''
 while b'\n' not in b:
  part=sock.recv(65536)
  if not part:raise ConnectionError('Probe closed')
  b+=part
 return json.loads(b.split(b'\n')[0])
with (out/'run.log').open('w') as log:
 command=[str(exe),str(a.rom.resolve()),'--no-launcher','--input-script','probe.input','--widescreen','16:9',
  '--target-fps','60','--max-frames',str(4000+max(0,a.cycles-1)*624)]
 if not a.light:command+=['--wav','generated.wav','--mem-write-log','0xFFFFE0-0xFFFFE5@4000']
 proc=subprocess.Popen(command,cwd=out,env=env,stdout=log,stderr=log,
  creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
 deadline=time.monotonic()+120+max(0,a.cycles-1)*12;sock=None;bridge=None;stalled=False
 observer=None
 if a.observer:
  from sonic2_stall_observer import start_observer
  observer=start_observer(proc.pid,exe,exe.with_suffix('.map'),a.observer,out/'host',
      seconds=int(120+max(0,a.cycles-1)*12),owned_child=proc,stacks=a.stack_snapshots)
 if os.name=='nt':
  from sonic2_audio_bridge_probe import BridgeProbe
  bridge=BridgeProbe(proc.pid,exe.with_suffix('.map'))
 try:
  while proc.poll() is None and time.monotonic()<deadline:
   if a.light:
    state=bridge.read() if bridge else None
    samples.append(dict(wall=time.monotonic(),bridge=state))
    if a.stall_ms and not stalled and state and state['pushed_frames']>=1400*3732:
     from sonic2_audio_bridge_probe import stall_own_producer
     tid=stall_own_producer(proc.pid,a.stall_ms/1000);stalled=True
     samples.append(dict(stall_ms=a.stall_ms,thread=tid,wall=time.monotonic()))
    time.sleep(.5);continue
   if not sock:
    try:sock=socket.create_connection(('127.0.0.1',a.port),timeout=1);sock.settimeout(3)
    except OSError:time.sleep(.25);continue
   try:
    stats=rpc(sock,'audio_stats');samples.append(dict(wall=time.monotonic(),stats=stats,bridge=bridge.read() if bridge else None))
    rpc(sock,'audio_delivery_dump',path=str(out/'delivery.txt'))
    if len(samples)%3==0:
     (out/'frame-performance.json').write_text(json.dumps(rpc(sock,'frame_performance')))
   except (OSError,ValueError,ConnectionError):
    sock.close();sock=None
   time.sleep(2)
  if proc.poll() is None:raise TimeoutError('Audio probe exceeded 120 seconds')
 finally:
  if sock:sock.close()
  if bridge:bridge.close()
 (out/'audio-stats.json').write_text(json.dumps(samples,indent=2)+'\n')
assert proc.returncode==0,(out/'run.log').read_text()[-3000:]
if observer:
 assert observer.wait(timeout=30)==0,'Host observer failed; see host-observer.log'
 from sonic2_stall_observer import summarize
 report=summarize(out/'host',exe.with_suffix('.map'))
 print('HOST STALLS',len(report['events']),'observer max gap ms',report['max_observer_gap_ms'],flush=True)
# Script EXIT returns before the runner's normal WAV finalizer. Preserve the
# captured samples and repair only the two RIFF length fields for inspection.
wav=out/'generated.wav'
if wav.exists():
 with wav.open('r+b') as stream:
  header=stream.read(44)
  assert header[:4]==b'RIFF' and header[8:12]==b'WAVE' and header[36:40]==b'data'
  size=wav.stat().st_size
  stream.seek(4);stream.write(struct.pack('<I',size-8))
  stream.seek(40);stream.write(struct.pack('<I',size-44))
print('CAPTURED',a.case,'stats',samples[-1] if samples else 'no TCP connection',flush=True)
