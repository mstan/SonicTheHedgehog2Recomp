"""Qualify native progress observation against known waiting and CPU stalls."""
import argparse,json,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sonic2_stall_observer import start_observer,summarize
p=argparse.ArgumentParser(description=__doc__)
for n in ('observer','fixture','out'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args(); a.out.mkdir(parents=True,exist_ok=False)
for stacks in (False,True):
    prefix=a.out/('stacks' if stacks else 'read-only')
    proc=subprocess.Popen([str(a.fixture.resolve())],creationflags=subprocess.CREATE_NO_WINDOW)
    watcher=start_observer(proc.pid,a.fixture,a.fixture.with_suffix('.map'),a.observer,prefix,10,
                           owned_child=proc,stacks=stacks,frame_symbol='host_fixture_frame')
    assert proc.wait(timeout=20)==0
    assert watcher.wait(timeout=30)==0
    report=summarize(prefix,a.fixture.with_suffix('.map'))
    relevant={e['frame']:e for e in report['events'] if e['frame'] in (40,80)}
    assert set(relevant)=={40,80},report
    assert relevant[40]['classification']=='mostly_waiting',relevant[40]
    assert relevant[80]['classification']=='mostly_cpu',relevant[80]
    for event in relevant.values():
        assert 200<event['duration_ms']<600,event
        if stacks:
            assert event['pcs'],event
            expected='fixture_wait' if event['frame']==40 else 'fixture_busy'
            assert any(expected in frame['symbol'] for frame in event['stack']),event
        else:assert not event['pcs'] and event['probe_pause_us']==0,event
    print(json.dumps({'stacks':stacks,'max_observer_gap_ms':report['max_observer_gap_ms'],'events':list(relevant.values())},indent=2),flush=True)
print('PASS waiting/CPU classification and own-child stack snapshots; live attach never suspends')
