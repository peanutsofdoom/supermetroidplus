// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only terrain kernel. No guest pointers are writable through this API.
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>

namespace SMPWide {
using Bytes=std::span<const uint8_t>;
// Retail 82:93A1 runs gameplay during post-pause fade-in; death states
// 19/20 also run gameplay while fading the room. Later death states do not.
// 1Bh runs the same gameplay draw path while automatic reserves refill health.
// 26h (82:84BD) still draws gameplay while the departing gunship fades out.
// Keep the room/ship presentation until27h initializes the ending cinematic.
inline bool gameplayPresentationState(unsigned state){return state==7 || state==8 || state==0x0c || state==0x12 || (state>=0x13 && state<=0x14) || state==0x1b || state==0x26 || state==0x29 || state==0x2a;}
inline uint16_t word(Bytes bytes, unsigned address) {
  if(address+1>=bytes.size()) throw std::out_of_range("terrain word");
  return uint16_t(bytes[address]) | uint16_t(bytes[address+1])<<8;
}
struct Pixel {
  uint16_t color=0;
  uint8_t priority=0;
  bool opaque=false,valid=false;
};
class Terrain {
  Bytes ram,vram,cgram;
  unsigned base,width,height,layer;
public:
  Terrain(Bytes r,Bytes v,Bytes c,unsigned tileBase,unsigned roomLayer=0):ram(r),vram(v),cgram(c),base(tileBase),layer(roomLayer) {
    if(layer>1)throw std::invalid_argument("unknown room layer");
    if(r.size()!=131072 || v.size()!=65536 || c.size()!=512)
      throw std::invalid_argument("incomplete immutable terrain images");
    if(base>=65536 || base%8192) throw std::invalid_argument("invalid tile base");
    width=word(r,0x7a5);height=word(r,0x7a7);
    if(!width || !height || uint64_t(width)*height>0x3200)
      throw std::invalid_argument("invalid room dimensions");
  }
  bool entry(int x,int y,uint16_t& result) const {
    if(x<0 || y<0 || unsigned(x)>=width*16 || unsigned(y)>=height*16) return false;
    unsigned index=unsigned(y/16)*width+unsigned(x/16);
    unsigned block=word(ram,(layer?0x19602:0x10002)+index*2);
    unsigned px=x%16,py=y%16;
    if(block&0x400) px=15-px;
    if(block&0x800) py=15-py;
    unsigned quadrant=(py/8)*2+px/8;
    result=word(ram,0xa000+(block&1023)*8+quadrant*2)^((block&0xc00)*16);
    return true;
  }
  Pixel pixel(int x,int y) const {
    uint16_t tile;
    if(!entry(x,y,tile)) return {};
    return pixelEntry(tile,x,y);
  }
  Pixel pixelEntry(uint16_t tile,int x,int y) const {
    unsigned px=unsigned(x)&7,py=unsigned(y)&7;
    if(tile&0x4000) px=7-px;
    if(tile&0x8000) py=7-py;
    unsigned address=base+(tile&1023)*32,index=0;
    for(unsigned plane=0;plane<4;plane++)
      index|=((vram[(address+(plane/2)*16+py*2+plane%2)&65535]>>(7-px))&1)<<plane;
    unsigned palette=index ? ((tile>>10)&7)*16+index : 0;
    return {uint16_t(word(cgram,palette*2)&32767),uint8_t((tile>>13)&1),bool(index),true};
  }
};
}
