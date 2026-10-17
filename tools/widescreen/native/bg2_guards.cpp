// SPDX-License-Identifier: GPL-3.0-or-later
// Independent colors and signed geometry expose wrong layer/map selection.
#include "scene_geometry.hpp"
#include <iostream>
#include <vector>
int main() {
  using namespace SMPWide;
  try {
    std::vector<uint8_t> ram(131072),old(131072),vram(65536),palette(512);
    auto put=[](auto& data,unsigned a,unsigned v){data[a]=v;data[a+1]=v>>8;};
    for(auto* data:{&ram,&old}) {put(*data,0x7a5,32);put(*data,0x7a7,16);}
    put(ram,0x79b,0x97b5);put(ram,0x7bb,0x97c6);put(ram,0x998,8);
    put(ram,0x921,256);put(old,0x79b,0x975c);put(old,0x7bb,0x976d);
    // BG1 block 0 is red, custom BG2 block 1 green; retained map is blue.
    for(unsigned q=0;q<4;q++)put(ram,0xa008+q*2,0x2001);
    put(ram,0x19602+(2*32+1)*2,1);
    for(unsigned a=0x4000;a<0x4800;a+=2)put(old,a,0x2002);
    for(unsigned y=0;y<8;y++) {vram[y*2]=255;vram[32+y*2+1]=255;vram[64+y*2]=255;vram[64+y*2+1]=255;}
    put(palette,2,31);put(palette,4,31<<5);put(palette,6,31<<10);
    const auto original=ram,originalOld=old,originalVram=vram,originalPalette=palette;
    unsigned checks=0;
    auto require=[&](bool condition){if(!condition)throw std::runtime_error("BG2 guard failed");checks++;};
    SceneGeometry ordinary(ram,ram,{},vram,palette,0,false,0,0);
    auto green=ordinary.background(0,32,272,0,0,0,0);
    require(green && green->valid && green->color==(31<<5) && green->priority==1);
    auto outside=ordinary.background(-17,32,272,0,0,0,0);require(outside && !outside->valid);
    put(ram,0x998,11);put(ram,0x911,65520);
    SceneGeometry joined(ram,ram,old,vram,palette,0,true,512,0);
    green=joined.background(32,32,240,0,0,0,0);require(green && green->color==(31<<5));
    auto blue=joined.background(0,32,240,0,0,0,0);require(blue && blue->color==(31<<10));
    blue=joined.background(-43,32,240,0,0,0,0);require(blue && blue->color==(31<<10));
    // Distinct metatile quadrants independently expose all four block flips.
    for(unsigned q=0;q<4;q++)put(ram,0xa000+q*2,100+q);
    for(unsigned flip=0;flip<4;flip++) {
      put(ram,0x19602,flip<<10);Terrain t(ram,vram,palette,0,1);uint16_t entry=0;
      require(t.entry(0,0,entry) && entry==((100+flip)^(flip<<14)));
    }
    bool rejected=false;try {Terrain invalid(ram,vram,palette,0,2);}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected);require(old==originalOld && vram==originalVram && palette==originalPalette);
    // Changes above were fixture setup only; preserve the original copy too.
    require(word(original,0x998)==8);
    auto prior=original,arrival=originalOld;
    put(prior,0x7bb,0x97e0);put(arrival,0x7bb,0x9787);
    put(arrival,0x998,11);put(arrival,0x791,1);put(arrival,0x927,512);put(arrival,0x911,512);
    SceneGeometry departing(arrival,arrival,prior,vram,palette,0,true,0,0);
    green=departing.background(272,32,0,0,0,512,0);
    require(green && green->valid && green->color==(31<<5));
    require(!departing.background(255,32,0,0,0,512,0));
    // Display camera, not BG2's incoming-room parallax, selects the old side.
    green=departing.background(248,32,100,0,0,536,0);
    require(green && green->color==(31<<5));
    put(arrival,0x7bb,0xffff);
    SceneGeometry unknown(arrival,arrival,prior,vram,palette,0,true,0,0);
    rejected=false;try{unknown.background(272,32,0,0,0,512,0);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected);
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
