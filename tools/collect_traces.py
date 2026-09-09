"""Fetch completed RAM traces once, through encrypted ESPHome API; no HA writes."""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time
from aioesphomeapi import APIClient
from analyze_traces import read_summary

async def collect(args):
    config=json.loads(args.connection.read_text())
    client=APIClient(config['host'],6053,None,noise_psk=config['api_key'])
    output=args.output
    output.mkdir(parents=True,exist_ok=True);output.chmod(0o700)
    state_path=output/'collector-state.json'
    prior=json.loads(state_path.read_text()) if state_path.exists() else {}
    fetched=0;lost=[]
    await client.connect(login=True)
    try:
        info,_,services=await client.device_info_and_list_entities()
        service=next(s for s in services if s.name=='get_transition_trace')
        async def request(sequence,offset):
            before=time.time()
            response=await client.execute_service(service,{'sequence':sequence,'offset':offset},return_response=True,timeout=15)
            after=time.time()
            if not response or not response.success: raise RuntimeError('Trace action failed')
            data=json.loads(response.response_data)
            if data.get('error'): raise RuntimeError(data['error'])
            if data.get('schema')!=1: raise RuntimeError('Unsupported trace schema')
            return data,(before+after)/2,after-before
        index,_,_=await request(0,0)
        boot=index['boot_id']
        if not re.fullmatch(r'[0-9a-f]{16}',boot): raise RuntimeError('Invalid device boot identifier')
        directory=output/'traces'/boot
        directory.mkdir(parents=True,exist_ok=True)
        last=prior.get('boots',{}).get(boot,0)
        if index['oldest']>last+1:
            lost.append({'boot_id':boot,'first_missing':last+1,'last_missing':index['oldest']-1})
        for sequence in sorted(index['ready_ids']):
            if not isinstance(sequence,int) or sequence<=0: raise RuntimeError('Invalid trace sequence')
            dest=directory/f'{sequence:06d}.json'
            if dest.exists(): continue
            offset=0;events=[];header=None
            while True:
                data,midpoint,rtt=await request(sequence,offset)
                if data['boot_id']!=boot or data['id']!=sequence or data['offset']!=offset:
                    raise RuntimeError('Device rebooted or trace changed during collection')
                if header is None:
                    header={k:v for k,v in data.items() if k not in ('events','offset','next_offset')}
                    age=((data['uptime_ms'] & 0xffffffff)-data['trigger_ms']) & 0xffffffff
                    header['trigger_utc']=datetime.fromtimestamp(midpoint-age/1000,timezone.utc).isoformat()
                    header['clock_round_trip_seconds']=rtt
                    header['firmware_compiled']=info.compilation_time
                    header['collected_at']=datetime.now(timezone.utc).isoformat()
                if len(data['events'])>16: raise RuntimeError('Oversized trace page')
                events.extend(data['events'])
                next_offset=data['next_offset']
                if next_offset==-1: break
                if next_offset<=offset or next_offset>256: raise RuntimeError('Invalid pagination')
                offset=next_offset
                await asyncio.sleep(0.05)
            if len(events)!=header['count']: raise RuntimeError('Incomplete trace download')
            header['events']=events
            tmp=dest.with_suffix('.tmp');tmp.write_text(json.dumps(header,indent=2));tmp.chmod(0o600);tmp.replace(dest)
            fetched+=1
        saved=[int(p.stem) for p in directory.glob('*.json')]
        prior.setdefault('boots',{})[boot]=max(saved,default=last)
        prior['last_checked']=datetime.now(timezone.utc).isoformat()
        prior['last_index']=index
        if lost: prior.setdefault('loss_reports',[]).extend(lost)
        temp=state_path.with_suffix('.tmp');temp.write_text(json.dumps(prior,indent=2));temp.chmod(0o600);temp.replace(state_path)
    finally:
        await client.disconnect()
    summary=read_summary(output)
    summary.update(last_checked=prior['last_checked'],new_traces=fetched,pending=index['pending'],
                   loss_reports=prior.get('loss_reports',[]),target_cycles=args.target_cycles,
                   target_reached=summary['complete_cycles']>=args.target_cycles)
    temp=output/'summary.tmp';temp.write_text(json.dumps(summary,indent=2));temp.chmod(0o600);temp.replace(output/'summary.json')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('cycles','unknowns')},indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--connection',type=Path,required=True,help='Private JSON with host and api_key')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--target-cycles',type=int,default=20)
    asyncio.run(collect(parser.parse_args()))
