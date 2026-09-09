"""Summarize complete idle -> heating -> idle cycles from exported trace windows."""
import argparse
from collections import Counter, defaultdict
from datetime import datetime
import json
from pathlib import Path

STATES={0:'Unknown',1:'Not running',2:'Running',3:'Fault',255:'Startup'}
REASONS={0:'none',1:'acquiring',2:'frame_gap',3:'stale_images',4:'missing_flashes',
         5:'solid_light',6:'irregular_or_acquiring_pattern',7:'wrong_jpeg_size',
         8:'jpeg_decode_error',9:'classification_disabled'}

def summarize(traces):
    boots=defaultdict(list)
    for t in traces: boots[t['boot_id']].append(t)
    cycles=[]; unknowns=[]; gaps=[]; truncated=0
    for boot, items in boots.items():
        last_id=None; known=None; pending=[]; active=None
        for t in sorted(items,key=lambda x:x['id']):
            if last_id is not None and t['id'] != last_id+1:
                gaps.append({'boot_id':boot,'after':last_id,'before':t['id']})
                known=None;pending=[];active=None
            last_id=t['id']
            bad=bool(t.get('dropped') or t.get('pre_truncated'))
            truncated+=bad
            if t['from']==255:
                known=None;pending=[];active=None
            state=t['to']
            if state==0:
                reason=REASONS.get(t['reason'],'unrecognized_reason')
                # Pattern reasons can be overwritten during reacquisition; retain raw gap evidence.
                evidence=Counter(str(e[1]) for e in t.get('events',[]) if e[1] in (4,5))
                unknowns.append({'boot_id':boot,'id':t['id'],'time':t['trigger_utc'],
                                 'reason':reason,'raw_error_events':dict(evidence)})
                pending.append(t)
                continue
            if state==3:
                known=None;pending=[];active=None
                continue
            if state not in (1,2): continue
            if state==2 and known==1:
                active={'boot_id':boot,'start_id':t['id'],'started_utc':t['trigger_utc'],
                        'start_trigger_ms':t.get('trigger_ms'),
                        'start_unknown_ids':[u['id'] for u in pending],
                        'start_unknown_seconds':_duration(pending,t),
                        'complete_trace':not bad and not any(u.get('dropped') or u.get('pre_truncated') for u in pending)}
            elif state==1 and known==2 and active:
                active.update(stop_id=t['id'],stopped_utc=t['trigger_utc'],
                              stop_unknown_ids=[u['id'] for u in pending],
                              stop_unknown_seconds=_duration(pending,t))
                active['complete_trace'] &= not bad and not any(u.get('dropped') or u.get('pre_truncated') for u in pending)
                active['running_seconds']=_elapsed({'trigger_ms':active['start_trigger_ms'],'trigger_utc':active['started_utc']},t)
                cycles.append(active);active=None
            known=state;pending=[]
    usable=[c for c in cycles if c['complete_trace']]
    return {'trace_count':len(traces),'boot_count':len(boots),'complete_cycles':len(usable),
            'cycles_with_incomplete_traces':len(cycles)-len(usable),
            'starts_with_unknown':sum(bool(c['start_unknown_ids']) for c in usable),
            'stops_with_unknown':sum(bool(c['stop_unknown_ids']) for c in usable),
            'unknown_reason_counts':dict(Counter(u['reason'] for u in unknowns)),
            'sequence_gaps':gaps,'truncated_traces':truncated,'cycles':cycles,'unknowns':unknowns}

def _duration(pending,current):
    return _elapsed(pending[0],current) if pending else 0

def _elapsed(first,last):
    if first.get('trigger_ms') is not None and last.get('trigger_ms') is not None:
        return ((last['trigger_ms']-first['trigger_ms']) & 0xffffffff)/1000
    return (datetime.fromisoformat(last['trigger_utc'])-datetime.fromisoformat(first['trigger_utc'])).total_seconds()

def read_summary(directory):
    return summarize([json.loads(p.read_text()) for p in Path(directory).glob('traces/*/*.json')])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args=parser.parse_args()
    print(json.dumps(read_summary(args.directory),indent=2))
