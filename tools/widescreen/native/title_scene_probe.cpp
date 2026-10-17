// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_scene.hpp"
#include <vector>
#include <iostream>
#include <cassert>
using namespace SMPWide;
int main() {
  std::vector<uint8_t> v(65536),p(512),r(205),out(342*224*3,123);
  auto put=[&](Reg key,unsigned value){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=value>>(8*i);};
  put(Reg::bgMode,7);put(Reg::m7a,256);put(Reg::m7d,256);
  put(Reg::aboveEnable,1);put(Reg::displayBrightness,15);
  // Coordinate-coded eight-pixel repeating texture, independently expected.
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)v[2*(y*8+x)+1]=x+1;
  for(unsigned x=1;x<=8;x++)p[x*2]=x;
  Registers regs(r);
  for(int x=-43;x<299;x++) {
    int src=x<0?-x-1:x>255?511-x:x;
    unsigned expected=((((src&7)+1)*15*16+15)/30)<<16;
    assert(titleColor(v,p,regs,x,8)==expected);
  }
  assert(titleRow(v,p,r,1,out.data()));
  for(unsigned x=43;x<299;x++)for(unsigned c=0;c<3;c++)assert(out[x*3+c]==123);
  put(Reg::screen_bg1_colorEnable,1);put(Reg::screen_colorRed,31);
  assert(titleColor(v,p,Registers(r),-1,8)==0xf80000);
  put(Reg::screen_colorMode,1);assert(titleColor(v,p,Registers(r),-1,8)==0);
  put(Reg::bgMode,1);assert(!titleRow(v,p,r,1,out.data()));
  put(Reg::displayDisable,1);assert(titleRow(v,p,r,1,out.data()));
  assert(!titleRow(v,p,r,225,out.data()));
  std::cout<<"PASS title identity transform, bounded side bays, native center exclusion, lighting and unsupported guards\n";
}
