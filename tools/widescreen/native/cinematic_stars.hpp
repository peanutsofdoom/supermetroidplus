// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "registers.hpp"
#include <array>
#include <algorithm>
namespace SMPWide {
struct CinematicStar {int x=0,y=0;unsigned attr=0,size=8;};
// Continue a native shooting star only after it crosses the original edge.
// This presentation-only tail leaves the native40-star loop and RNG untouched.
class CinematicStars {
  struct State {int64_t x=0,y=0;int vx=0,vy=0;unsigned phase=0;int timer=0;bool valid=false;};
  std::array<State,40> previous{},tails{};
  std::array<int,40> ax{},ay{},timers{};
  std::array<CinematicStar,40> visible{};
  unsigned visibleCount=0;
  bool configured=false;
  void advance(State& s,unsigned i) const {
    int scale=(s.phase&0xff00)>=0x400?2:1;
    s.vx=int16_t(s.vx+ax[i]*scale);s.vy=int16_t(s.vy+ay[i]*scale);
    s.x+=int64_t(s.vx)*256;s.y+=int64_t(s.vy)*256;
    if(--s.timer<=0){s.timer=timers[i];s.phase+=0x200;}
  }
public:
  void configure(Bytes rom) {
    if(rom.size()!=3145728)throw std::invalid_argument("shooting star ROM shape");
    for(unsigned i=0;i<40;i++) {
      unsigned a=0x5e9cf+i*8;
      ax[i]=int16_t(word(rom,a));ay[i]=int16_t(word(rom,a+2));timers[i]=word(rom,a+4);
    }
    configured=true;reset();
  }
  void reset(){previous={};tails={};visibleCount=0;}
  // Called at8B:E978, after native star motion, clipping and OAM construction.
  void observe(Bytes ram) {
    if(!configured || ram.size()!=131072 || word(ram,0x998)!=0x27 || !word(ram,0x1a05)){reset();return;}
    visibleCount=0;
    for(unsigned i=0;i<40;i++) {
      auto& tail=tails[i];
      if(tail.valid)advance(tail,i);
      unsigned a=0xe0c+i*16;
      State now{int64_t(int16_t(word(ram,a+2)))*65536+word(ram,a+4),
        int64_t(int16_t(word(ram,a+6)))*65536+word(ram,a+8),
        int16_t(word(ram,a+12)),int16_t(word(ram,a+14)),word(ram,a),int16_t(word(ram,a+10)),true};
      const auto& prev=previous[i];
      bool restarted=(now.phase&0xff00)==0 && now.x==128*65536 && now.y==128*65536 && !now.vx && !now.vy;
      if(prev.valid && !(prev.phase&0x8000) && (prev.phase&0xff00)>=0x200 && restarted) {
        State next=prev;advance(next,i);
        int x=int(next.x>>16),y=int(next.y>>16);
        if((x<4 || x>=256) && y>=4 && y<256){tail=next;tail.valid=true;}
      }
      if(tail.valid) {
        int x=int(tail.x>>16)-4,y=int(tail.y>>16)-4;
        if(x+8<=-43 || x>=299 || y+8<=0 || y>=224)tail.valid=false;
        else visible[visibleCount++]={x,y,0x09f0+std::min(3u,((tail.phase>>8)/2)-1)};
      }
      previous[i]=now;
    }
  }
  std::span<const CinematicStar> objects() const{return {visible.data(),visibleCount};}
};
}
