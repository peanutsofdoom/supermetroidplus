#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x7a5,64);put(ram,0x7a7,64);
  for(unsigned y=0;y<8;y++)vram[y*2]=255;
  put(pal,2,31);reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);
  reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,4);
  auto owner=std::make_shared<const OwnerImage>(1,ram);
  SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
  for(int dx=-3;dx<=3;dx++)for(int dy=-3;dy<=3;dy++) {
    reg(regs,Reg::hoffset,uint16_t(dx));reg(regs,Reg::voffset,uint16_t(dy));
    RowTimeline row(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,32);row.finish();
    if(row.at(256).cx!=dx || row.at(256).cy!=dy)throw std::runtime_error("signed shake coordinate mismatch");
    // Right margin must remain red room terrain when the camera shakes left
    // or up through zero. Old unsigned65535 coordinates made it all invalid.
    for(int x=256;x<299;x++) {
      auto p=row.pixel(x);if(!p.valid || p.rgb!=std::array<uint8_t,3>{248,0,0})throw std::runtime_error("shaken margin disappeared");
    }
  }
  if(sceneCoordinate(65535,0,SceneKind::Right)!=65535)throw std::runtime_error("door convention changed");
  std::cout<<"PASS:49shake offsets,2107margin pixels and preserved door convention\n";
}
