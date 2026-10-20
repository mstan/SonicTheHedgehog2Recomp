"""Real boot regression for default-off diagnostic I/O and explicit RAM capture."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('release','development','rom','config','out'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args(); a.out.mkdir(parents=True,exist_ok=False)
results=[]
for name,source,request in [('release-default',a.release,False),('release-request',a.release,True),
                            ('development-default',a.development,False),('development-request',a.development,True)]:
    folder=(a.out/name).resolve(); folder.mkdir()
    exe=folder/source.name; shutil.copy2(source,exe); shutil.copy2(source.parent/'SDL2.dll',folder/'SDL2.dll')
    shutil.copy2(a.config,folder/'sonic2-party.ini')
    (folder/'settings.ini').write_text('[audio]\nvolume=0\n[video]\nfullscreen=0\n')
    (folder/'test.input').write_text('WAIT 600\nPRESS START 2\nWAIT 40\nPRESS START 2\nWAIT 450\nASSERT_RAM8 FFF600 0C\nDUMP_RAM explicit.bin\nWAIT 2\nEXIT\n')
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software',GENESIS_STRICT_JSR_STACK='1')
    for key in ('GENESIS_YIELD_LOG','GENESIS_AUTO_RAMDUMP','GENESIS_FRAME_LOG','GENESIS_AUDIO_WRITEDUMP'):env.pop(key,None)
    if request:env.update(GENESIS_YIELD_LOG=str(folder/'requested-yield.log'),GENESIS_AUTO_RAMDUMP='1')
    with (folder/'run.log').open('w') as log:
        run=subprocess.run([str(exe),str(a.rom.resolve()),'--no-launcher','--turbo','--input-script','test.input','--max-frames','1200'],
            cwd=folder,env=env,stdout=log,stderr=log,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
    assert run.returncode==0,(folder/'run.log').read_text()[-3000:]
    assert (folder/'explicit.bin').stat().st_size==65536,'manual capture contract broken'
    expected=name=='development-request'
    assert (folder/'requested-yield.log').exists()==expected,name
    assert (folder/'ramdump_native.bin').exists()==expected,name
    assert not (folder/'yield_log_native.log').exists(),name
    results.append(dict(case=name,manual_ram=True,automatic_streams=expected))
    print('PASS',name,flush=True)
(a.out/'report.json').write_text(json.dumps(results,indent=2)+'\n')
