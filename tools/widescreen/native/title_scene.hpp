// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "registers.hpp"
#include <algorithm>
namespace SMPWide {
// Read-only extension of the title's 256x256 laboratory texture. Continue the
// outer wall/floor bays by reflecting only the outer43columns; the unique
// chamber, terminals, logo and copyright are never duplicated into new bays.
inline int titleTextureX(int x) {
  if(x>=-43 && x<0)return -x-1;
  if(x>=256 && x<299)return 511-x;
  return x;
}
inline unsigned titleColor(Bytes v,Bytes p,const Registers& r,int x,int y) {
  auto get=[&](Reg k){return r.get(k);};
  if(get(Reg::displayDisable))return 0;
  auto signed13=[](int n){return (n&4096)?(n&8191)-8192:n&8191;};
  auto clip=[](int n){return n&8192?n|~1023:n&1023;};
  int a=int16_t(get(Reg::m7a)),b=int16_t(get(Reg::m7b));
  int c=int16_t(get(Reg::m7c)),d=int16_t(get(Reg::m7d));
  int cx=signed13(get(Reg::m7x)),cy=signed13(get(Reg::m7y));
  int h=clip(signed13(get(Reg::hoffsetMode7))-cx),vv=clip(signed13(get(Reg::voffsetMode7))-cy);
  if(get(Reg::hflipMode7))x=255-x;
  if(get(Reg::vflipMode7))y=255-y;
  int px=((a*h&~63)+(b*vv&~63)+(b*y&~63)+cx*256+a*x)>>8;
  int py=((c*h&~63)+(d*vv&~63)+(d*y&~63)+cy*256+c*x)>>8;
  // A bounded continuation, not the Mode7 map's uninitialized outer tiles.
  unsigned index=0;
  if(get(Reg::aboveEnable) && px>=-43 && px<299 && py>=0 && py<256) {
    px=titleTextureX(px);
    unsigned tile=v[2*((py/8)*128+px/8)];
    index=v[2*(tile*64+(py&7)*8+(px&7))+1];
  }
  unsigned color=word(p,index*2)&32767;
  if(get(index?Reg::screen_bg1_colorEnable:Reg::screen_back_colorEnable)) {
    unsigned second=get(Reg::screen_colorRed)|(get(Reg::screen_colorGreen)<<5)|(get(Reg::screen_colorBlue)<<10),out=0;
    for(unsigned shift:{0,5,10}) {
      int n=(color>>shift)&31,q=(second>>shift)&31;
      n=get(Reg::screen_colorMode)?std::max(0,n-q):n+q;
      if(get(Reg::screen_colorHalve))n/=2;
      out|=std::min(31,n)<<shift;
    }
    color=out;
  }
  unsigned rgb=0;
  for(unsigned i=0;i<3;i++)rgb|=((((color>>(i*5))&31)*get(Reg::displayBrightness)*16+15)/30)<<(16-8*i);
  return rgb;
}
inline bool titleRow(Bytes v,Bytes p,Bytes regs,unsigned line,uint8_t* out) {
  if(v.size()!=65536 || p.size()!=512 || regs.size()!=205 || line<1 || line>224 || !out)return false;
  Registers r(regs);auto get=[&](Reg k){return r.get(k);};
  if(!get(Reg::displayDisable)) {
    if(get(Reg::bgMode)!=7 || get(Reg::extbg) || get(Reg::pseudoHires) || get(Reg::interlace) ||
       get(Reg::mosaicEnable) || get(Reg::screen_directColor) || get(Reg::screen_blendMode) ||
       get(Reg::window_bg1_oneEnable) || get(Reg::window_bg1_twoEnable) ||
       get(Reg::window_col_aboveMask) || get(Reg::window_col_belowMask))return false;
  }
  for(int x=-43;x<299;x++) {
    if(x>=0 && x<256)continue;
    auto color=titleColor(v,p,r,x,line);
    for(unsigned i=0;i<3;i++)out[((line-1)*342+x+43)*3+i]=color>>(16-8*i);
  }
  return true;
}
}
