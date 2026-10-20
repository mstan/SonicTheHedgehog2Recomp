"""Observe frame progress/CPU/faults/I/O of an exact Windows playtest binary.

Attach mode never suspends, writes target memory, changes audio or closes the
game. Optional native stacks are available through the own-child API only.
"""
import argparse
import bisect
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

def process_exe(pid):
    k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[c.c_uint32,c.c_int,c.c_uint32]; k.OpenProcess.restype=c.c_void_p
    k.QueryFullProcessImageNameW.argtypes=[c.c_void_p,c.c_uint32,c.c_wchar_p,c.POINTER(c.c_uint32)]
    k.CloseHandle.argtypes=[c.c_void_p]
    h=k.OpenProcess(0x1000,False,pid)
    if not h:raise OSError(c.get_last_error(),'OpenProcess(query)')
    try:
        path=c.create_unicode_buffer(32768); size=c.c_uint32(len(path))
        if not k.QueryFullProcessImageNameW(h,0,path,c.byref(size)):
            raise OSError(c.get_last_error(),'QueryFullProcessImageName')
        return Path(path.value)
    finally:k.CloseHandle(h)

def start_observer(pid,exe,map_path,observer,prefix,seconds=60,owned_child=None,stacks=False,frame_symbol='s_current_frame_for_input'):
    if os.name!='nt':raise OSError('This observer uses Windows process APIs')
    if stacks and (owned_child is None or owned_child.pid!=pid or owned_child.poll() is not None):
        raise ValueError('Native stacks may suspend only a child this caller just launched')
    exe=Path(exe).resolve(); map_path=Path(map_path).resolve(); prefix=Path(prefix).resolve()
    actual=process_exe(pid); binary=exe.read_bytes()
    digest=hashlib.sha256(binary).hexdigest()
    if hashlib.sha256(actual.read_bytes()).hexdigest()!=digest:
        raise ValueError('Live process does not match the supplied executable')
    text=map_path.read_text()
    preferred=int(re.search(r'Preferred load address is\s+([0-9a-fA-F]+)',text)[1],16)
    match=re.search(r'^\s*\S+\s+'+re.escape(frame_symbol)+r'\s+([0-9a-fA-F]+)\b',text,re.M)
    if not match:raise ValueError(f'Progress symbol {frame_symbol} missing from linker map')
    frame=int(match[1],16)
    ram=re.search(r'^\s*\S+\s+g_ram\s+([0-9a-fA-F]+)\s+',text,re.M)
    tag_rva=int(ram[1],16)-preferred+0xf600 if ram else 0
    stamp=int(re.search(r'Timestamp is\s+([0-9a-fA-F]+)',text)[1],16)
    pe=struct.unpack_from('<I',binary,0x3c)[0]
    if stamp!=struct.unpack_from('<I',binary,pe+8)[0]:raise ValueError('Linker map timestamp does not match executable')
    if struct.unpack_from('<H',binary,pe+4)[0]!=0x8664:raise ValueError('Expected x64 executable')
    prefix.parent.mkdir(parents=True,exist_ok=True)
    meta=dict(pid=pid,exe=str(actual),exe_sha256=digest,map=str(map_path),map_sha256=hashlib.sha256(map_path.read_bytes()).hexdigest(),
              frame_rva=frame-preferred,seconds=seconds,stacks=stacks,read_only=not stacks)
    prefix.with_name(prefix.name+'-identity.json').write_text(json.dumps(meta,indent=2)+'\n')
    log=prefix.with_name(prefix.name+'-observer.log').open('w')
    command=[str(Path(observer).resolve()),str(pid),hex(frame-preferred),str(seconds),str(prefix)]
    command+=['owned-child-stacks' if stacks else 'read-only',hex(tag_rva)]
    try:
        child=subprocess.Popen(command,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
    finally:log.close()
    return child

def summarize(prefix,map_path):
    prefix=Path(prefix); report=json.loads(prefix.with_name(prefix.name+'-events.json').read_text())
    text=Path(map_path).read_text(); preferred=int(re.search(r'Preferred load address is\s+([0-9a-fA-F]+)',text)[1],16)
    symbols=[]
    for line in text.splitlines():
        m=re.match(r'\s*\S+:\S+\s+(\S+)\s+([0-9a-fA-F]{16})\s+',line)
        if m:symbols.append((int(m[2],16)-preferred,m[1]))
    symbols.sort(); offsets=[x[0] for x in symbols]
    for event in report['events']:
        duration=event['duration_ms']; cpu=event['main_cpu_ms']
        event['classification']='mostly_waiting' if cpu<duration*.25 else 'mostly_cpu' if cpu>duration*.7 else 'mixed'
        event['main_cpu_percent']=round(100*cpu/duration,2) if duration else 0
        event['unpaused_duration_lower_bound_ms']=round(max(0,duration-event['probe_pause_us']/1000),3)
        event['stack']=[]
        for index,pc in enumerate(event['pcs']):
            relative=pc-report['base']; i=bisect.bisect_right(offsets,relative)-1
            # A nearest map symbol is useful only within the game's image.
            name=f'{symbols[i][1]}+0x{relative-offsets[i]:x}' if i>=0 and 0<=relative<0x1000000 else 'system/library address'
            native_symbols=event.get('symbols',[])
            if index<len(native_symbols) and native_symbols[index]:name=native_symbols[index]
            event['stack'].append(dict(pc=hex(pc),symbol=name))
        event['game_mode']={0:'sega',4:'title',8:'demo',12:'level'}.get(event.get('tag'),'other/unknown')
    report['limitations']=['Page-fault counts include soft faults; they do not establish hard paging.',
        'CPU counters have OS granularity; main-thread cycles are also retained.',
        'Observer scheduling/unwind gaps can hide stalls; inspect max_observer_gap_ms and sample times.',
        'Native snapshots pause only tool-owned children; probe_pause_us records that cost.']
    prefix.with_name(prefix.name+'-report.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pid',type=int,required=True)
    for n in ('exe','map','observer','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--seconds',type=int,default=60)
    a=p.parse_args()
    observer=start_observer(a.pid,a.exe,a.map,a.observer,a.out,a.seconds)
    status=observer.wait()
    if status:raise RuntimeError(f'Observer exited {status}; see {a.out}-observer.log')
    report=summarize(a.out,a.map)
    print(json.dumps({k:report[k] for k in ('pid','samples','max_observer_gap_ms','events')},indent=2))

if __name__=='__main__':main()
