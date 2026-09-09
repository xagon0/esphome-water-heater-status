from datetime import datetime, timedelta, timezone
import unittest
from tools.analyze_traces import summarize, _elapsed

def trace(seq,source,target,seconds,reason=0,boot='a',**extra):
    return dict(id=seq,boot_id=boot,**{'from':source,'to':target},reason=reason,
                trigger_utc=(datetime(2026,1,1,tzinfo=timezone.utc)+timedelta(seconds=seconds)).isoformat(),
                events=[],dropped=0,pre_truncated=False,**extra)

class TraceAnalysisTest(unittest.TestCase):
    def test_device_clock_handles_wrap_without_host_clock_jitter(self):
        first={'trigger_ms':0xfffff000,'trigger_utc':'2026-01-01T00:00:00+00:00'}
        last={'trigger_ms':(0xfffff000+9000)&0xffffffff,'trigger_utc':'2026-01-01T00:00:59+00:00'}
        self.assertEqual(_elapsed(first,last),9)

    def test_counts_clean_cycles_and_unknown_bridges(self):
        data=[trace(1,255,0,0),trace(2,0,1,12),trace(3,1,0,100,2),
              trace(4,0,2,109),trace(5,2,0,500,6),trace(6,0,1,504),
              trace(7,1,2,800),trace(8,2,1,1200)]
        report=summarize(data)
        self.assertEqual(report['complete_cycles'],2)
        self.assertEqual(report['starts_with_unknown'],1)
        self.assertEqual(report['stops_with_unknown'],1)
        self.assertEqual(report['cycles'][0]['start_unknown_seconds'],9)
        self.assertEqual(report['cycles'][0]['stop_unknown_seconds'],4)
    def test_does_not_join_across_missing_traces_or_reboots(self):
        data=[trace(1,0,1,0),trace(2,1,2,100),trace(4,2,1,500),
              trace(1,255,0,600,boot='b'),trace(2,0,2,620,boot='b'),trace(3,2,1,900,boot='b')]
        report=summarize(data)
        self.assertEqual(report['complete_cycles'],0)
        self.assertEqual(len(report['sequence_gaps']),1)
    def test_fault_and_incomplete_trace_do_not_count_toward_target(self):
        data=[trace(1,0,1,0),trace(2,1,2,100),trace(3,2,3,200),trace(4,3,1,300),
              trace(5,1,2,400),trace(6,2,1,600)]
        data[-1]['dropped']=2
        report=summarize(data)
        self.assertEqual(report['complete_cycles'],0)
        self.assertEqual(report['cycles_with_incomplete_traces'],1)

if __name__=='__main__': unittest.main()
