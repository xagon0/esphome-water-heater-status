#include "blink_decoder.h"
#include <cassert>
#include <iostream>
int main() {
  for(unsigned n:{2u,4u,5u,7u,8u}) {
    for(unsigned gap:{3000u,4000u}) {
      FaultDecoder d;uint32_t cycle=(n-1)*1000+gap;
      for(uint32_t t=0;t<4*cycle+1800;t+=20){uint32_t p=t%cycle;d.sample(t,p<(n-1)*1000+400 && p%1000>=200 && p%1000<300);}
      assert(d.active(4*cycle+1780));assert(d.code()==n);
      assert(!d.active(4*cycle+2500)); // stale images are not heater faults
    }
  }
  for(unsigned pattern=0;pattern<6;++pattern) {
    FaultDecoder d;
    for(uint32_t t=0;t<90000;t+=20) {
      uint32_t p=t%3060;bool lit=false;
      if(pattern==0)lit=p>=200&&p<300;
      if(pattern==1)lit=(p>=200&&p<280)||(p>=380&&p<460);
      if(pattern==2)lit=t%1000>=200&&t%1000<300;
      if(pattern==3)lit=true;
      if(pattern==4) {p=t%6000;lit=p<2400&&p%1000>=200&&p%1000<300;} // unsupported three-flash code
      d.sample(t,lit);assert(!d.active(t));
    }
  }
  FaultDecoder partial;
  for(uint32_t t=0;t<3800;t+=20)partial.sample(t,(t>=200&&t<300)||(t>=1200&&t<1300));
  assert(!partial.active(3780));
  FaultDecoder wrap;
  for(uint32_t t=0;t<24000;t+=20) {uint32_t p=t%4000;wrap.sample(0xfffff000u+t,(p>=200&&p<300)||(p>=1200&&p<1300));}
  assert(wrap.active(0xfffff000u+23980));
  wrap.sample(0xfffff000u+25000,true);assert(!wrap.active(0xfffff000u+25000));
  std::cout<<"Passed documented fault codes, normal single/strobe rejection, unsupported codes, solid/dark images, partial groups, stale frames, gaps and clock wraparound\n";
}
