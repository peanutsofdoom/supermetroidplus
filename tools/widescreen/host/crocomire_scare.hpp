// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
// Experimental, owner-approved nonlethal outro contact. $0DFC is cleared by
// retail new-game/demo setup and otherwise never read/written by the fixed ROM.
// Keeping the marker/countdown in WRAM makes save-state restore deterministic.
constexpr unsigned SmpCrocoScareState=0x0dfc, SmpCrocoScareMagic=0xc500;
template<class R> bool smpCrocoScareProtected(R r) {
  unsigned s=r(SmpCrocoScareState);
  return (s&0xff00)==SmpCrocoScareMagic && (s&255)!=0;
}
template<class R> bool smpCrocoScareSequence(R r) {
  unsigned stage=r(0xfa8);
  return r(0x79b)==0xa98d && r(0xf78)==0xddbf && stage>=0x44 && stage<=0x48 && !(stage&1);
}
template<class R,class W> void smpCrocoScareFloor(R r,W w) {
  unsigned game=r(0x998);
  if((game==8 || game==12 || game==18) && smpCrocoScareProtected(r) && int16_t(r(0x9c2))<=0) {
    w(0x9c2,1);w(0xa4c,0);
  }
}
template<class R,class ROM,class W> void smpCrocoScareTick(R r,ROM rom,W w) {
  unsigned game=r(0x998);
  if(game!=8 && game!=12 && game!=18)return;
  if(r(0xa78))return; // No countdown/collisions while time is frozen.
  bool protectedNow=smpCrocoScareProtected(r),sequence=smpCrocoScareSequence(r);
  if(!protectedNow && game==8 && sequence && int16_t(r(0x9c2))>0) {
    int sx=r(0xaf6),sy=r(0xafa),rx=r(0xafe),ry=r(0xb00);
    bool hit=false;
    for(unsigned slot=0;slot<36 && !hit;slot+=2)if(r(0x1997+slot)==0x90c1) {
      unsigned map=r(0x1b6b+slot);if(map<0x8000)continue;
      unsigned at=0x68000+(map&32767),count=rom(at)|(rom(at+1)<<8);
      if(count>128 || map+2+count*5>65536)continue;
      // Match the visible43px-left relocation; decorative native radius is0.
      int ox=int(r(0x1a4b+slot))-43,oy=r(0x1a93+slot);
      for(unsigned n=0;n<count;n++) {
        unsigned p=at+2+n*5,raw=rom(p)|(rom(p+1)<<8),size=raw&0x8000?16:8;
        int dx=int(raw&511);if(dx>=256)dx-=512;
        int x=ox+dx,y=oy+int(int8_t(rom(p+2)));
        if(sx-rx<x+int(size) && sx+rx>x && sy-ry<y+int(size) && sy+ry>y){hit=true;break;}
      }
    }
    if(hit) {
      w(0x9c2,1);w(0xa4c,0);w(SmpCrocoScareState,SmpCrocoScareMagic|120);
      if(r(0x18a8)<60)w(0x18a8,60);
      protectedNow=true;
    }
  }
  if(protectedNow) {
    smpCrocoScareFloor(r,w);
    // Refill only at expiry so native blinking continues normally.
    if(r(0x18a8)<=1)w(0x18a8,60);
    unsigned ticks=r(SmpCrocoScareState)&255;
    if(sequence)ticks=120;else --ticks;
    w(SmpCrocoScareState,ticks?SmpCrocoScareMagic|ticks:0);
  }
}
