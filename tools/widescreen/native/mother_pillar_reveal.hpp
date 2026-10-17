// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "registers.hpp"
#include "../host/mother_brain_arena.hpp"
#include <array>
#include <algorithm>
#include <vector>
namespace SMPWide {
// One-shot presentation for the owner's widened Mother Brain chamber. Reuse
// retail86:E208 /8D:B406 explosion maps and the disappearing pillar's pixels.
// Nothing is spawned in guest projectile slots, and no collision/timer changes.
class MotherPillarReveal {
  int age=-1;
  bool seen=false,started=false,capture=false;
  mutable bool havePillar=false;
  mutable std::array<uint8_t,16*192*3> pillar{};
  static int signedBits(unsigned n,unsigned bits){unsigned mask=(1u<<bits)-1;n&=mask;return n&(1u<<(bits-1))?int(n)-int(mask+1):int(n);}
public:
  void reset(){age=-1;seen=started=capture=havePillar=false;}
  void observe(Bytes ram) {
    bool room=word(ram,0x998)==8 && word(ram,0x911)==0 && smpMotherBrainRoom([&](unsigned a){return word(ram,a);});
    if(!room){reset();return;}
    bool expanded=smpMotherBrainArena([&](unsigned a){return word(ram,a);});
    unsigned function=word(ram,0xfa8);
    bool preparing=word(ram,0x7800)==1 && (function==0x8c87 || function==0x8c9e);
    if(seen && !started && preparing){age=0;started=true;}
    else if(age>=0 && age<48)age++;
    // Loading a checkpoint inside the reveal must not manufacture a new blast.
    if(!seen && (expanded || preparing))started=true;
    capture=!expanded && !started;seen=true;
  }
  int frame() const{return age;}
  bool active() const{return age>=0 && age<48;}
  void retain(const std::vector<uint8_t>& rgb) const {
    if(!capture)return;
    for(unsigned y=32;y<224;y++)for(unsigned x=0;x<16;x++)for(unsigned c=0;c<3;c++)
      pillar[((y-32)*16+x)*3+c]=rgb[(y*342+240+43+x)*3+c];
    havePillar=true;
  }
  // Draw only the effect's occupied scanlines, using that scanline's actual
  // native VRAM, palette and brightness. The HUD is never part of the effect.
  void row(std::vector<uint8_t>& rgb,unsigned y,Bytes rom,Bytes vram,Bytes palette,Bytes regs,int origin) const {
    if(!active() || y<31 || y>=224 || rom.size()!=3145728)return;
    Registers r(regs);if(r.get(Reg::displayDisable) || !r.get(Reg::displayBrightness))return;
    auto paint=[&](int x,unsigned color){if(x<0 || x>=342)return;for(unsigned c=0;c<3;c++)rgb[(y*342+x)*3+c]=((((color>>(c*5))&31)*r.get(Reg::displayBrightness)*16+15)/30);};
    // Small textured fragments come from the last intact pillar, not new art.
    if(havePillar && age>=8 && age<42)for(int i=0;i<14;i++) {
      int t=age-8,side=i&1?1:-1,sy=40+(i/2)*24;
      int dx=244+(i&1)*8+side*(t*(2+i%3)/5),dy=sy-t/2+t*t/28;
      int row=int(y)-dy;if(row<0 || row>=6)continue;
      for(int x=0;x<4;x++) {
        auto at=((sy-32+row)*16+(i&1)*8+x)*3;
        if(at<0 || at+2>=int(pillar.size()))continue;
        if(!(pillar[at]|pillar[at+1]|pillar[at+2]))continue;
        int outX=dx+x+origin;if(outX<0 || outX>=342)continue;
        for(unsigned c=0;c<3;c++)rgb[(y*342+outX)*3+c]=pillar[at+c];
      }
    }
    static constexpr int delays[]={0,5,2,7,1,6,3};
    for(unsigned burst=0;burst<7;burst++) {
      int t=age-delays[burst];if(t<0 || t>=30)continue;
      // Six five-frame poses, exactly as in the native big-explosion list.
      unsigned pointer=word(rom,0x36208+(unsigned(t)/5)*4+2);
      unsigned map=0x68000+(pointer&32767),count=word(rom,map);if(count>16)continue;
      int bx=248+(int(burst%3)-1)*3,by=48+int(burst)*24;
      for(unsigned n=0;n<count;n++) {
        unsigned at=map+2+n*5,raw=word(rom,at),attr=word(rom,at+3),size=raw&0x8000?16:8;
        int ox=bx+signedBits(raw,9),oy=by+signedBits(rom[at+2],8),py=int(y)-oy;
        if(py<0 || py>=int(size))continue;
        if(attr&0x8000)py=size-1-py;
        for(unsigned x=0;x<size;x++) {
          unsigned px=attr&0x4000?size-1-x:x;
          unsigned base=r.get(Reg::obj_tiledataAddress);if(attr&256)base+=(1+r.get(Reg::obj_nameselect))<<12;
          unsigned tile=(((attr&255)/16+unsigned(py)/8)&15)*16+((attr+px/8)&15);
          unsigned addr=((base+tile*16)&32767)*2,index=0;
          for(unsigned b=0;b<4;b++)index|=((vram[(addr+b/2*16+(py%8)*2+b%2)&65535]>>(7-px%8))&1)<<b;
          if(index)paint(ox+int(x)+origin,word(palette,(128+((attr>>9)&7)*16+index)*2));
        }
      }
    }
  }
  void shake(std::vector<uint8_t>& rgb) const {
    if(!active())return;
    static constexpr int horizontal[]={0,1,-1,2,-2,1,0,-1};
    int strength=age<24?1:age<38?2:4,dx=horizontal[age%8]/strength;
    int dy=age<24 && age%6==0?1:age<24 && age%6==3?-1:0;
    if(!dx && !dy)return;
    auto original=rgb;
    for(int y=31;y<224;y++)for(int x=0;x<342;x++) {
      int sx=std::clamp(x-dx,0,341),sy=std::clamp(y-dy,31,223);
      for(unsigned c=0;c<3;c++)rgb[(y*342+x)*3+c]=original[(sy*342+sx)*3+c];
    }
  }
};
}
