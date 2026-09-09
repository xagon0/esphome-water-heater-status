#include "transition_recorder.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace heater_trace;

int main() {
  auto r=std::make_unique<Recorder>();
  assert(r->observe(0,Status::UNKNOWN,Reason::ACQUIRING,0,80));
  assert(!r->observe(1,Status::UNKNOWN,Reason::PATTERN,0,80));
  for (uint32_t t=1000;t<=25000;t+=1000) r->record({t,2700,0,10,Kind::HEALTH,1});
  assert(r->observe(25000,Status::IDLE,Reason::NONE,0,80));
  const auto *idle=r->get(2);
  assert(idle && idle->events[0].at==10000 && !idle->ready);
  r->record({26000,3050,1,500,Kind::RISE,0});
  r->observe(27000,Status::UNKNOWN,Reason::FRAME_GAP,0,80);
  r->observe(36000,Status::RUNNING,Reason::NONE,0,80);
  r->record({45000,2700,0,10,Kind::HEALTH,1});
  r->tick(45001);
  assert(idle->ready && idle->events[idle->count-1].at==45000);
  const auto saved=idle->count;
  r->record({46000,2700,0,10,Kind::HEALTH,1});
  assert(idle->count==saved); // completed pages cannot change during export
  const auto *unknown=r->get(3);
  assert(unknown->reason==Reason::FRAME_GAP && unknown->from==Status::IDLE);
  assert(r->get(4)->from==Status::UNKNOWN && r->get(4)->to==Status::RUNNING);
  assert(r->get(0)==nullptr && r->get(999)==nullptr);

  auto full=std::make_unique<Recorder>();
  for (uint32_t t=0;t<18000;t+=30) full->record({t,0,0,0,Kind::HEALTH,0});
  full->observe(18000,Status::UNKNOWN,Reason::PATTERN,0,80);
  for (uint32_t t=18030;t<=39000;t+=30) full->record({t,0,0,0,Kind::HEALTH,0});
  const auto *truncated=full->get(1);
  assert(truncated->ready && truncated->pre_truncated);
  assert(truncated->count==Recorder::EVENTS && truncated->dropped>0);

  auto rolling=std::make_unique<Recorder>();
  for (uint32_t i=0;i<70;++i)
    rolling->observe(i*25000,i%2 ? Status::RUNNING : Status::IDLE,Reason::NONE,0,80);
  assert(rolling->newest()==70 && rolling->oldest()==7 && rolling->overwritten()==6);
  assert(!rolling->get(6) && rolling->get(7) && rolling->get(70));

  auto wrap=std::make_unique<Recorder>();
  const uint32_t start=0xfffff000u;
  wrap->record({start,3000,1,500,Kind::RISE,0});
  wrap->observe(start+2000,Status::RUNNING,Reason::NONE,0,80);
  wrap->record({start+4000,100,1,0,Kind::FALL,0});
  wrap->tick(start+22001);
  assert(wrap->get(1)->ready && wrap->get(1)->events[0].at==start);
  assert(status_code("Not running")==Status::IDLE);
  assert(reason_code("Camera unavailable")==Reason::STALE);
  assert(reason_code("JPEG decode failed")==Reason::JPEG_DECODE);
  std::cout << "Passed pre/post windows, overlapping transitions, immutable completed traces, overflow, eviction, duplicate suppression and timer wraparound\n";
}
