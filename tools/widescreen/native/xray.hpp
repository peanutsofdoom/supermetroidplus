// SPDX-License-Identifier: GPL-3.0-or-later
// Passive extension of retail91:C5FF..C9D4 and91:CDD6..D2D6.
#pragma once
#include "terrain.hpp"
#include <algorithm>
#include <optional>
namespace SMPWide {
inline bool xrayOwned(Bytes r) {
  if(r.size()!=131072 || !word(r,0xa78))return false;
  for(unsigned s=0;s<12;s+=2)if(word(r,0x18b4+s)) {
    unsigned b=word(r,0x18fc+s)&255,p=word(r,0x18f0+s);
    if((b==0x88 && p==0x86ef) || (b==0x91 && p==0xd27f))return true;
  }
  return false;
}
inline unsigned xrayRomAddress(unsigned bank,unsigned p){return (bank&127)*32768+(p&32767);}
struct XrayCone {
  int x=0,y=0;unsigned angle=64,width=0;
  static int floor256(int n){return n>=0?n/256:-((-n+255)/256);}
  std::pair<int,int> edges(int row,Bytes rom) const {
    constexpr int inf=32768;
    int lo=int(angle)-int(width),hi=int(angle)+int(width);
    if(lo<0)lo+=256;if(hi>256)hi-=256;
    auto slope=[&](int a){a%=128;return int(word(rom,xrayRomAddress(0x91,0xc9d4)+unsigned(a)*2));};
    int a=slope(lo),b=slope(hi),d=row-(y-1),origin=x*256;
    int kind=(!width && (angle==64 || angle==192))?4:
      lo<128?(lo>=64?2:hi>=64?0:1):(lo>=192?1:hi>=192?3:2);
    if(kind==4)return d==0?(angle==64?std::pair{x,inf}:std::pair{-inf,x}):std::pair{1,0};
    if(kind==0)return {d<0?floor256(origin+a*(-d)):d>0?floor256(origin+b*d):x,inf};
    if(kind==3)return {-inf,d<0?floor256(origin-b*(-d)):d>0?floor256(origin-a*d):x};
    if(kind==1){
      if(d>0)return {1,0};if(!d)return {x,x};d=-d;
      if(lo<192)return {floor256(origin+a*d),floor256(origin+b*d)};
      if(hi<192)return {floor256(origin-a*d),floor256(origin+b*d)};
      return {floor256(origin-a*d),floor256(origin-b*d)};
    }
    if(d<0)return {1,0};if(!d)return {x,x};
    if(hi<128)return {floor256(origin+b*d),floor256(origin+a*d)};
    if(lo<128)return {floor256(origin-b*d),floor256(origin+a*d)};
    return {floor256(origin-b*d),floor256(origin-a*d)};
  }
};

class XrayTerrain {
  Bytes ram,rom;Terrain terrain;
  unsigned width,height;
  uint16_t rw(unsigned bank,unsigned p) const {return word(rom,xrayRomAddress(bank,p));}
  std::optional<unsigned> program(unsigned index) const {
    unsigned type=word(ram,0x10002+index*2)&0xf000,bts=ram[0x16402+index];
    for(unsigned i=0;i<32;i++) {
      unsigned p=0xd2d6+i*4,t=rw(0x91,p);if(t==0xffff)break;
      if(t!=type)continue;
      unsigned list=rw(0x91,p+2);
      for(unsigned j=0;j<256;j++) {unsigned v=rw(0x91,list+j*4);if(v==0xffff)break;if(v==0xff00 || v==bts)return rw(0x91,list+j*4+2);}
      break;
    }
    return {};
  }
  std::optional<unsigned> replacement(unsigned index,int dx,int dy) const {
    auto p=program(index);if(!p)return {};
    unsigned fn=rw(0x91,*p);
    if(fn==0xce79 || fn==0xcebb) {
      if(dx || dy)return {};
      int x=int(index%width),y=int(index/width);bool vertical=fn==0xce79;
      for(unsigned n=0;n<width*height;n++) {
        int step=int(int8_t(ram[0x16402+index]));if(!step)break;
        (vertical?y:x)+=step;
        if(x<0 || y<0 || x>=int(width) || y>=int(height))return 0xff;
        index=unsigned(y)*width+unsigned(x);unsigned type=word(ram,0x10002+index*2)&0xf000;
        if(type==0xd000){vertical=true;continue;}
        if(type==0x5000){if(int8_t(ram[0x16402+index])<0)vertical=false;continue;}
        break;
      }
      if((word(ram,0x10002+index*2)&0xf000)!=rw(0x91,0xd2da))return {};
      auto q=program(index);return q?std::optional<unsigned>(rw(0x91,*q+2)):std::nullopt;
    }
    if(fn==0xcf3e && word(ram,0x79f)!=1)return {};
    if(fn==0xcf36 || fn==0xcf3e)return !dx&&!dy?std::optional<unsigned>(rw(0x91,*p+2)):std::nullopt;
    if(fn==0xcf4e && !dy && dx<=1)return rw(0x91,*p+2+dx*2);
    if(fn==0xcf62 && !dx && dy<=1)return rw(0x91,*p+2);
    if(fn==0xcf6f && dx<=1 && dy<=1)return rw(0x91,*p+2+(dy*2+dx)*2);
    return {};
  }
public:
  XrayTerrain(Bytes r,Bytes ro,Bytes v,Bytes p,unsigned base):ram(r),rom(ro),terrain(r,v,p,base),width(word(r,0x7a5)),height(word(r,0x7a7)){}
  std::optional<uint16_t> entry(int x,int y) const {
    uint16_t tile;if(!terrain.entry(x,y,tile))return {};
    int bx=x/16,by=y/16;std::optional<unsigned> block;
    for(int sy=std::max(0,by-1);sy<=by;sy++)for(int sx=std::max(0,bx-1);sx<=bx;sx++)
      if(auto v=replacement(unsigned(sy)*width+unsigned(sx),bx-sx,by-sy))block=v;
    // Item PLMs and per-room special cases override ordinary block reveals.
    for(int s=78;s>=0;s-=2)if(word(ram,0x1c37+s)>=0xdf89 && word(ram,0x1c87+s)/2==unsigned(by)*width+unsigned(bx)) {
      unsigned bit=word(ram,0x1dc7+s),offset=word(ram,0x1e17+s);
      if(bit<512 && !(ram[0xd870+bit/8]&(1u<<(bit&7))) && offset<16) {
        unsigned draw=rw(0x84,0x839d+offset);block=rw(0x84,draw+2)&0xfff;
      }
    }
    unsigned special=rw(0x8f,word(ram,0x7bb)+16);
    if(special>=0x8000)for(unsigned i=0;i<512;i++) {
      unsigned a=special+i*6,xx=rw(0x8f,a),yy=rw(0x8f,a+2);if(!xx && !yy)break;
      if(xx==unsigned(bx) && yy==unsigned(by))block=rw(0x8f,a+4);
    }
    if(block) {
      unsigned py=unsigned(y)&15;if(*block&0x800)py=15-py;
      tile=word(ram,0xa000+(*block&1023)*8+(py/8*2+(unsigned(x)&15)/8)*2);
      // Retail91:D0A6 swaps the metatile's rows without flipping each8px tile.
    }
    return tile;
  }
  Pixel pixel(int x,int y) const {auto t=entry(x,y);return t?terrain.pixelEntry(*t,x,y):Pixel{};}
};
}
