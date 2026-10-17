// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cinematic_stars.hpp"
#include "../host/ceres_arena.hpp"
namespace SMPWide {
// The relocated wall is outside retail's enemy drawing range. Read its actual
// spritemap and visibility; only restore its missing margin OBJ presentation.
class CeresCinematicWall {
  std::array<CinematicStar,13> map{},visible{};
  unsigned count=0;
public:
  void configure(Bytes rom) {
    const unsigned base=0x26*32768+0x7b2f;
    if(word(rom,base)!=13)throw std::invalid_argument("Ceres wall map");
    for(unsigned i=0;i<13;i++) {
      unsigned a=base+2+i*5,x=word(rom,a)&511;
      map[i]={x>=256?int(x)-512:int(x),int(int8_t(rom[a+2])),word(rom,a+3),16};
    }
  }
  void observe(Bytes ram) {
    count=0;
    if(!smpCeresArena([&](unsigned a){return word(ram,a);}))return;
    for(unsigned a=0xf78;a<0x1778;a+=64) {
      if(word(ram,a)!=0xe23f || word(ram,a+60)!=6 || word(ram,a+2)!=328 ||
         (word(ram,a+14)&0x300) || word(ram,a+22)!=0xfb2f)continue;
      for(auto q:map) {
        q.x+=int(word(ram,a+2))-int(word(ram,0x911));
        q.y+=int(word(ram,a+6))-int(word(ram,0x915));
        q.attr=(q.attr&~0x0e00u)|(word(ram,a+30)&0x0e00u);
        visible[count++]=q;
      }
      break;
    }
  }
  std::span<const CinematicStar> objects() const{return {visible.data(),count};}
};
}

