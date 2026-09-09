#include "blink_decoder.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>
using Pattern = std::function<bool(uint32_t)>;
bool is(const char *a,const char *b) { return std::strcmp(a,b)==0; }
void check(const char *expected,Pattern lit,uint32_t start=0) {
  BlinkDecoder d;
  for(uint32_t t=0;t<=24000;t+=20) d.sample(start+t,lit(t));
  assert(is(d.state(start+24000),expected));
}
int main() {
  auto standby=[](uint32_t t){uint32_t p=t%3060;return p>=200 && p<300;};
  auto heating=[](uint32_t t){uint32_t p=t%3060;return (p>=200 && p<300)||(p>=400 && p<500);};
  check("Standby",standby);check("Heating",heating);
  check("Heating",heating,0xfffff000u);
  check("Standby",standby,0xfffff000u);
  for(unsigned flashes=2;flashes<=10;++flashes) {
    BlinkDecoder d;
    for(uint32_t t=0;t<90000;t+=20) {
      uint32_t p=t%(flashes*1000+3000);
      d.sample(t,p<flashes*1000 && p%1000>=200 && p%1000<300);
      assert(!is(d.state(t),"Heating") && !is(d.state(t),"Standby"));
    }
  }
  // Reject both the old 1-second heating interpretation and 4-second idle.
  for(unsigned period:{1000u,4000u}) {
    BlinkDecoder d;
    for(uint32_t t=0;t<60000;t+=20) {d.sample(t,t%period>=200&&t%period<300);assert(!is(d.state(t),"Heating")&&!is(d.state(t),"Standby"));}
  }
  BlinkDecoder d;
  for(uint32_t t=0;t<=24000;t+=20)d.sample(t,heating(t));
  assert(is(d.state(25000),"Camera unavailable"));
  for(uint32_t t=24020;t<=30000;t+=20)d.sample(t,false);
  assert(!is(d.state(30000),"Heating"));
  for(uint32_t t=30020;t<=56000;t+=20)d.sample(t,standby(t));
  assert(is(d.state(56000),"Standby"));
  d.sample(57000,true);assert(is(d.state(57000),"Frame gap"));
  for(uint32_t t=57020;t<=58000;t+=20)d.sample(t,true);
  assert(is(d.state(58000),"LED continuously on"));
  // A single missed half-strobe does not erase a confirmed result; a
  // sustained normal single-flash pattern still becomes Standby.
  BlinkDecoder missed;
  for(uint32_t t=0;t<=24000;t+=20)missed.sample(t,heating(t));
  for(uint32_t t=24020;t<=28000;t+=20) {missed.sample(t,standby(t));assert(is(missed.state(t),"Heating"));}
  for(uint32_t t=28020;t<=45000;t+=20)missed.sample(t,standby(t));
  assert(is(missed.state(45000),"Standby"));
  BlinkDecoder partial;
  for(uint32_t t=0;t<=6500;t+=20)partial.sample(t,heating(t));
  assert(!is(partial.state(6500),"Heating"));
  std::cout<<"Passed WT8840 single/strobe patterns, four-group acquisition, 2-10 flash faults, old scheme rejection, stale frames, missing pulses, heat-to-idle, frame gaps, solid light and millis wraparound\n";
}
