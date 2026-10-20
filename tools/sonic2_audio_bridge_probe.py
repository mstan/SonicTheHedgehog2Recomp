"""Read-only Windows diagnostic for a probe's exact executable/linker map.

No target memory is written. Validate the ABI against the initialized bridge
configuration before interpreting counters. Not part of the game build.
"""
import ctypes as c
from pathlib import Path
import re
import time

def stall_own_producer(pid,seconds):
    """Fault injection for the tool's own child: pause its earliest thread.

    Audio callbacks keep running. Always resume the selected thread, including
    on exceptions. The caller passes only the Popen PID it just created.
    """
    k=c.WinDLL('kernel32',use_last_error=True)
    class Thread(c.Structure):
        _fields_=[(n,c.c_uint32) for n in ('size','usage','tid','pid')]+[
            ('base',c.c_int32),('delta',c.c_int32),('flags',c.c_uint32)]
    k.CreateToolhelp32Snapshot.argtypes=[c.c_uint32,c.c_uint32];k.CreateToolhelp32Snapshot.restype=c.c_void_p
    k.Thread32First.argtypes=k.Thread32Next.argtypes=[c.c_void_p,c.POINTER(Thread)]
    k.OpenThread.argtypes=[c.c_uint32,c.c_int,c.c_uint32];k.OpenThread.restype=c.c_void_p
    k.GetThreadTimes.argtypes=[c.c_void_p]+[c.c_void_p]*4
    k.CloseHandle.argtypes=[c.c_void_p]
    k.SuspendThread.argtypes=k.ResumeThread.argtypes=[c.c_void_p]
    k.SuspendThread.restype=k.ResumeThread.restype=c.c_uint32
    snap=k.CreateToolhelp32Snapshot(4,0);entry=Thread();entry.size=c.sizeof(entry);threads=[]
    try:
        more=k.Thread32First(snap,c.byref(entry))
        while more:
            if entry.pid==pid:
                h=k.OpenThread(0x42,False,entry.tid)
                if h:
                    stamps=[c.c_uint64() for _ in range(4)]
                    if k.GetThreadTimes(h,*[c.byref(v) for v in stamps]):threads.append((stamps[0].value,entry.tid,h))
                    else:k.CloseHandle(h)
            more=k.Thread32Next(snap,c.byref(entry))
        assert threads,'No producer thread found'
        _,tid,handle=min(threads)
        assert k.SuspendThread(handle)!=0xFFFFFFFF,'SuspendThread failed'
        try:time.sleep(seconds)
        finally:assert k.ResumeThread(handle)!=0xFFFFFFFF,'ResumeThread failed'
        return tid
    finally:
        for _,_,h in threads:k.CloseHandle(h)
        k.CloseHandle(snap)

class Config(c.Structure):
    _fields_=[('channels',c.c_int),('source_rate',c.c_double),('host_rate',c.c_double),
        ('taps',c.c_int),('phases',c.c_int)]+[(n,c.c_double) for n in
        ('target_ms','ring_ms','kp','max_correction','err_lp_ms','slew_pp_per_s','deadband_ms','em_low_ms','em_high_ms','preroll_ms')]+[
        ('stretch_enable',c.c_int)]+[(n,c.c_double) for n in ('stretch_min_ms','stretch_max_ms','stretch_xfade_ms')]
class Stats(c.Structure):
    _fields_=[(n,c.c_uint64) for n in ('pushed_frames','pulled_frames','underrun_events','overflow_drops','stretch_frames','stretch_events')]+[
        ('last_fill_ms',c.c_double),('last_correction',c.c_double)]
class Bridge(c.Structure):
    _fields_=[('cfg',Config),('half',c.c_int),('coeffs',c.c_void_p),('ring',c.c_void_p),('cap',c.c_int64),
        ('in_count',c.c_uint64)]+[(n,c.c_double) for n in ('out_pos','cur_step','err_lp','corr','gain')]+[
        ('primed',c.c_int),('prime_ms',c.c_double),('last_out',c.c_float*2),('concealing',c.c_int)]+[
        (n,c.c_double) for n in ('loop_start','loop_len','loop_pos')]+[('stats',Stats)]

class BridgeProbe:
    def __init__(self,pid,map_path):
        text=Path(map_path).read_text()
        preferred=int(re.search(r'Preferred load address is\s+([0-9a-fA-F]+)',text)[1],16)
        symbol=int(re.search(r'^\s*\S+\s+s_bridge\s+([0-9a-fA-F]+)\s+audio.obj',text,re.M)[1],16)
        self.k=c.WinDLL('kernel32',use_last_error=True)
        self.k.OpenProcess.argtypes=[c.c_uint32,c.c_int,c.c_uint32];self.k.OpenProcess.restype=c.c_void_p
        self.k.K32EnumProcessModules.argtypes=[c.c_void_p,c.POINTER(c.c_void_p),c.c_uint32,c.POINTER(c.c_uint32)]
        self.k.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
        self.k.GetProcessTimes.argtypes=[c.c_void_p]+[c.c_void_p]*4
        self.k.CloseHandle.argtypes=[c.c_void_p]
        self.handle=self.k.OpenProcess(0x410,False,pid)
        if not self.handle:raise OSError(c.get_last_error(),'OpenProcess(read only)')
        modules=(c.c_void_p*1024)();needed=c.c_uint32()
        if not self.k.K32EnumProcessModules(self.handle,modules,c.sizeof(modules),c.byref(needed)):
            raise OSError(c.get_last_error(),'EnumProcessModules')
        self.address=modules[0]+symbol-preferred
    def read(self):
        b=Bridge();n=c.c_size_t()
        if not self.k.ReadProcessMemory(self.handle,self.address,c.byref(b),c.sizeof(b),c.byref(n)):return None
        if b.cfg.channels!=2 or b.cfg.taps!=32 or not 200000<b.cfg.source_rate<230000 or not 32000<=b.cfg.host_rate<=192000:
            return None # not initialized, or mismatched binary/map/ABI
        result={n:getattr(b.stats,n) for n,_ in Stats._fields_}
        result.update(fill_ms=max(0,b.in_count-b.out_pos)*1000/b.cfg.source_rate,concealing=b.concealing,
            source_rate=b.cfg.source_rate,host_rate=b.cfg.host_rate,cap=b.cap,primed=b.primed)
        stamps=[c.c_uint64() for _ in range(4)]
        if self.k.GetProcessTimes(self.handle,*[c.byref(v) for v in stamps]):
            result['process_cpu_us']=(stamps[2].value+stamps[3].value)//10
        return result
    def close(self):
        if self.handle:self.k.CloseHandle(self.handle);self.handle=None
