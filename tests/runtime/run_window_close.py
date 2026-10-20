"""Measure a real WM_CLOSE with an unread output pipe and populated hitch history.

Only launches/moves/stalls this tool's own muted child. Never touches owner games.
"""
import argparse,ctypes as c,json,os,shutil,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sonic2_audio_bridge_probe import stall_own_producer
p=argparse.ArgumentParser(description=__doc__)
for n in ('exe','rom','out'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--expected-title',required=True)
p.add_argument('--expect-blocked',action='store_true')
p.add_argument('--hitches',type=int,default=32)
p.add_argument('--debug-port',type=int,default=4957)
a=p.parse_args(); folder=a.out.resolve(); folder.mkdir(parents=True,exist_ok=False)
exe=folder/a.exe.name; shutil.copy2(a.exe,exe); shutil.copy2(a.exe.parent/'SDL2.dll',folder/'SDL2.dll')
(folder/'settings.ini').write_text('[video]\nfullscreen=0\n[audio]\nvolume=0\n')
(folder/'sonic2-party.ini').write_text('slots=1\nplayer1=sonic\nplayer2=none\namy_enabled=0\ns3k_enabled=0\nshadow_enabled=0\nmighty_enabled=0\nsave_menu_enabled=0\n')
u=c.WinDLL('user32',use_last_error=True)
callback=c.WINFUNCTYPE(c.c_int,c.c_void_p,c.c_ssize_t)
u.EnumWindows.argtypes=[callback,c.c_ssize_t]
u.GetWindowThreadProcessId.argtypes=[c.c_void_p,c.POINTER(c.c_uint32)]
u.GetClassNameW.argtypes=[c.c_void_p,c.c_wchar_p,c.c_int]
u.GetWindowTextW.argtypes=[c.c_void_p,c.c_wchar_p,c.c_int]
u.SetWindowPos.argtypes=[c.c_void_p,c.c_void_p,c.c_int,c.c_int,c.c_int,c.c_int,c.c_uint32]
u.PostMessageW.argtypes=[c.c_void_p,c.c_uint32,c.c_size_t,c.c_ssize_t]
u.IsWindowVisible.argtypes=u.IsWindow.argtypes=[c.c_void_p]
env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='wasapi',LNG_SCRIPT='observe',LNG_TEST_HIDDEN='1')
for n in ('GENESIS_HOST_TIMING_DUMP','GENESIS_FRAME_LOG','GENESIS_YIELD_LOG','GENESIS_AUDIO_WRITEDUMP','SDL_RENDER_DRIVER'):env.pop(n,None)
command=[str(exe),str(a.rom.resolve()),'--no-launcher','--target-fps','60','--port',str(a.debug_port)]
proc=subprocess.Popen(command,cwd=folder,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
hwnd=None; report={}
try:
    deadline=time.monotonic()+20
    while proc.poll() is None and time.monotonic()<deadline and not hwnd:
        found=[]
        @callback
        def visit(window,_):
            pid=c.c_uint32(); u.GetWindowThreadProcessId(window,c.byref(pid))
            if pid.value==proc.pid:
                name=c.create_unicode_buffer(128); u.GetClassNameW(window,name,len(name))
                if name.value=='SDL_app':found.append(window)
            return 1
        u.EnumWindows(visit,0)
        if found:hwnd=found[0]
        else:time.sleep(.01)
    if not hwnd:
        if proc.poll() is None:proc.terminate()
        output,_=proc.communicate(timeout=10);(folder/'failed-run.log').write_bytes(output)
        raise AssertionError(f'Own child did not create a gameplay window; exit={proc.returncode}; {output[-1500:]!r}')
    title=c.create_unicode_buffer(512); u.GetWindowTextW(hwnd,title,len(title))
    if not a.expect_blocked:assert title.value==a.expected_title,title.value
    # Keep the real window visible to Windows, outside the owner's screen and
    # without activating it. Output remains unread to expose blocking flushes.
    assert u.SetWindowPos(hwnd,c.c_void_p(1),-12000,-12000,0,0,0x51)
    time.sleep(.3)
    for _ in range(a.hitches):
        assert proc.poll() is None
        stall_own_producer(proc.pid,.07);time.sleep(.025)
    visible=bool(u.IsWindowVisible(hwnd)); assert visible,'Fixture window must be observable before close'
    begin=time.perf_counter(); assert u.PostMessageW(hwnd,0x10,0,0)
    hidden_ms=None
    while proc.poll() is None and time.perf_counter()-begin<1.0:
        if hidden_ms is None and not u.IsWindowVisible(hwnd):hidden_ms=(time.perf_counter()-begin)*1000
        time.sleep(.005)
    blocked=proc.poll() is None
    elapsed=(time.perf_counter()-begin)*1000
    report=dict(title=title.value,hitches_injected=a.hitches,blocked_output=True,blocked_at_1000ms=blocked,
                close_observation_ms=round(elapsed,3),window_hidden_ms=round(hidden_ms,3) if hidden_ms is not None else None)
    # Preserve the measurement even when the expected close contract fails.
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    assert blocked==a.expect_blocked,report
    # Drain only after the measurement; this also lets the expected failing
    # baseline finish naturally rather than leaving a deliberately blocked app.
    output,_=proc.communicate(timeout=15)
    (folder/'run.log').write_bytes(output)
    assert proc.returncode==0,proc.returncode
finally:
    if proc.poll() is None:
        proc.terminate(); output,_=proc.communicate(timeout=10); (folder/'failed-run.log').write_bytes(output)
print(json.dumps(report),flush=True)
