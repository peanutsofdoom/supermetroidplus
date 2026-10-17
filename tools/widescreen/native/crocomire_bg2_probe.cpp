// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_geometry.hpp"
#include <iostream>
#include <vector>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){
  std::vector<uint8_t> ram(131072),vram(65536),pal(512);
  put(ram,0x998,8);put(ram,0x79b,0xa98d);put(ram,0xf78,0xddbf);
  put(ram,0x7a5,128);put(ram,0x7a7,16);ram[0x59]=0x49;
  // Deliberately fill both screens: wrapping must not expose either outside
  // the single body map, even when retail parks its scroll at 100h.
  for(unsigned t=0;t<2048;t++)put(vram,0x9000+t*2,1+(t/1024));
  for(unsigned y=0;y<8;y++){vram[32+y*2]=255;vram[64+y*2+1]=255;}
  put(pal,2,31);put(pal,4,992);
  unsigned checked=0;
  for(int offset:{-600,-344,-294,-284,-256,-1,0,179,255,256,511,550}) {
    put(ram,0x911,800);put(ram,0xf7a,851-offset);
    SceneGeometry scene(ram,ram,{},vram,pal,0,false,0,0);
    for(unsigned scroll:{unsigned(offset)&511,256u})for(int x=-43;x<299;x++) {
      auto p=scene.background(x,100,scroll,459,0,800,0);
      int px=x+offset;bool visible=px>=0 && px<512;
      if(!p || !p->valid || p->opaque!=visible ||
          (visible && p->color!=(px<256?31:992)))
        throw std::runtime_error("Crocomire body wrapped or misplaced");
      checked++;
    }
  }
  put(ram,0xf86,0x100);
  SceneGeometry invisible(ram,ram,{},vram,pal,0,false,0,0);
  auto hidden=invisible.background(280,100,256,459,0,800,0);
  if(!hidden || hidden->opaque)throw std::runtime_error("Hidden boss rendered");
  put(ram,0xf86,0);put(ram,0xf7a,1152);put(ram,0x911,807);
  auto previous=ram;
  put(previous,0x911,815);put(ram,0x911,818);
  SceneGeometry parkedPhase(ram,ram,{},vram,pal,0,false,0,0,{}, {},0,0,{},std::nullopt,previous);
  SceneGeometry nativePhase(ram,ram,{},vram,pal,0,false,0,0);
  for(int x=-43;x<299;x++) {
    auto parked=parkedPhase.background(x,100,256,459,0,818,0);
    // Give the ordinary path its matching camera for the exact native phase.
    auto refRam=ram;put(refRam,0x911,815);
    SceneGeometry ref(refRam,refRam,{},vram,pal,0,false,0,0);
    auto expected=ref.background(x,100,unsigned(815+51-1152)&511,459,0,815,0);
    if(!parked || !expected || parked->color!=expected->color || parked->opaque!=expected->opaque)
      throw std::runtime_error("Parked/native Crocomire camera phase differs");
    checked++;
  }
  put(ram,0x911,807);
  // Entry fade must use the same single-body placement as gameplay, rather
  // than repeating the parked map into the right margin before state8.
  put(ram,0x911,768);put(ram,0xf7a,1152);put(ram,0xfa8,0);put(ram,0x998,11);
  for(unsigned phase:{0xe664u,0xe6a2u,0xe737u}) {
    put(ram,0x99c,phase);SceneGeometry entry(ram,ram,{},vram,pal,0,false,0,0);
    for(int x=256;x<299;x++) {
      auto p=entry.background(x,100,256,459,0,768,0);
      if(!p || p->opaque)throw std::runtime_error("Premature wrapped Crocomire during fade");
    }
  }
  put(ram,0x99c,0xe353);SceneGeometry preload(ram,ram,{},vram,pal,0,false,0,0);
  if(preload.background(280,100,256,459,0,768,0))throw std::runtime_error("Preload incorrectly qualified");
  put(ram,0x998,8);put(ram,0x911,807);
  put(ram,0x911,807);
  for(unsigned death:{0x3eu,0x40u,0x44u,0x48u,0x52u,0x54u,0x56u,0x58u}) {
    put(ram,0xfa8,death);
    SceneGeometry skeleton(ram,ram,{},vram,pal,0,false,0,0);
    for(int x=-43;x<299;x++) {
      auto p=skeleton.background(x,100,0xfeef,0xff28,0,807,0);
      if(!p || !p->valid || p->opaque)
        throw std::runtime_error("Cleared body leaked during skeleton phase");
      checked++;
    }
  }
  put(ram,0xfa8,0);
  SceneGeometry otherScroll(ram,ram,{},vram,pal,0,false,0,0);
  if(otherScroll.background(280,100,0,459,0,807,0))
    throw std::runtime_error("Unrelated death-sequence scroll overridden");
  for(unsigned room:{0xa98cu,0xa98eu}) {
    put(ram,0x79b,room);
    SceneGeometry other(ram,ram,{},vram,pal,0,false,0,0);
    if(other.background(280,100,256,459,0,807,0))
      throw std::runtime_error("Unrelated room changed");
  }
  std::cout<<"PASS "<<checked<<" body samples; hidden boss, death scroll and room guards\n";
}
