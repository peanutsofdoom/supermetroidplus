// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_reveal.hpp"
#include <iostream>
#include <stdexcept>
int main(){
 SmpTitleSkip skip;
 if(skip.observe(1,0x9a47,0) || skip.observe(1,0x9a47,1))throw std::runtime_error("Premature skip");
 if(!skip.observe(1,0x9a47,3) || !skip.observe(1,0x9f29,0))throw std::runtime_error("Intro skip lost");
 if(skip.observe(4,0x9b68,0) || skip.observe(1,0x9b68,0))throw std::runtime_error("Skip reset");
 if(!skip.observe(1,0x9f52,0))throw std::runtime_error("Reveal skip lost");
 for(auto size:{std::pair{684u,224u},std::pair{2048u,448u},std::pair{684u,480u}}){
  unsigned w=size.first,h=size.second,crop=h==480?16:0,margin=w==2048?256:86;
  std::vector<uint32_t> base(w*h,0x123456),a;
  unsigned previous=0;
  for(unsigned age=0;age<=310;age++){
   a=base;smpTitleReveal(a,w,h,crop,0x9f29,900-age,15);unsigned visible=0;
   for(unsigned x=0;x<margin;x++)visible+=a[(h/2)*w+x]!=0;
   if(visible<previous || (age<=45 && visible) || (age>=285 && visible!=margin))throw std::runtime_error("Reveal progression");previous=visible;
   for(unsigned x=margin;x<w-margin;x++)if(a[(h/2)*w+x]!=base[(h/2)*w+x])throw std::runtime_error("Center rescaled");
  }
  a=base;smpTitleReveal(a,w,h,crop,0x9f29,500,15);if(a==base)throw std::runtime_error("Stamp absent");
  auto completed=a;
  for(auto fn:{0x9a47u,0x9f29u,0x9f52u}) {
    a=base;smpTitleReveal(a,w,h,crop,fn,900,15,true);
    if(a!=completed)throw std::runtime_error("Skipped title not complete");
  }
  auto repeated=base;smpTitleReveal(repeated,w,h,crop,0x9f29,500,15);if(a!=repeated)throw std::runtime_error("Nondeterministic restore");
  a=base;smpTitleReveal(a,w,h,crop,0x9f52,500,128);if(a!=base)throw std::runtime_error("Stamp during forced blank");
  a=base;smpTitleReveal(a,w,h,crop,0x9b68,0,15);if(a[0]!=0 || a[w/2]!=0x123456)throw std::runtime_error("Restart did not reset");
 }
 std::cout<<"PASS standard-width start, monotonic reveal, unchanged center, stamp, fade and reset at all surface sizes\n";
}
