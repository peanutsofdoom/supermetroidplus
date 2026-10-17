#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x79b,0xa4da);put(ram,0x7bb,0xa4e7);
  put(ram,0x7a5,64);put(ram,0x7a7,128);put(ram,0x921,512);put(ram,0x917,411);
  for(unsigned q=0;q<4;q++){put(ram,0xa008+q*2,1);put(ram,0xa010+q*2,2);}
  for(unsigned y=0;y<128;y++)for(unsigned x=0;x<64;x++)put(ram,0x19602+(y*64+x)*2,x<40?1:2);
  for(unsigned y=0;y<8;y++){vram[32+y*2]=255;vram[64+y*2+1]=255;vram[96+y*2]=vram[96+y*2+1]=255;}
  put(pal,2,992);put(pal,4,31);put(pal,6,31744);
  reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);
  reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,6);reg(regs,Reg::hoffset,549);
  reg(regs,Reg::bg2_aboveEnable,1);reg(regs,Reg::bg2_priority0,4);
  reg(regs,Reg::bg2_screenAddress,0x4800);reg(regs,Reg::bg2_hoffset,411+512);
  put(ram,0x79b,0xcfc9);put(ram,0x7bb,0xcfd6);put(ram,0x998,11);
  put(ram,0x791,7);put(ram,0x915,512);put(ram,0x929,512);put(ram,0x911,256);put(ram,0x927,256);
  reg(regs,Reg::hoffset,256);
  for(unsigned i=0;i<1024;i++)put(vram,0x9000+i*2,3);
  auto old=ram;put(old,0x79b,0xcefb);put(old,0x7bb,0xcf27);put(old,0x998,8);
  auto prior=std::make_shared<const OwnerImage>(1,old);
  unsigned checks=0;
  for(unsigned phase:{0xe664u,0xe6a2u,0xe737u}) {
    put(ram,0x99c,phase);auto owner=std::make_shared<const OwnerImage>(2,ram);
    SceneSelection scene{owner,owner,prior,SceneKind::Up,0,0};
    RowTimeline row(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,100);row.finish();
    for(int x=-43;x<0;x++){if(row.pixel(x).rgb!=std::array<uint8_t,3>{0,248,0})throw std::runtime_error("Arrival BG2 still depends on stale ring");checks++;}
  }
  // Scroll still in progress: preserve native transition path.
  put(ram,0x915,544);reg(regs,Reg::voffset,544);
  auto owner=std::make_shared<const OwnerImage>(3,ram);SceneSelection scene{owner,owner,prior,SceneKind::Up,0,0};
  RowTimeline before(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,100);before.finish();
  if(before.pixel(-20).rgb!=std::array<uint8_t,3>{0,0,248})throw std::runtime_error("Arrival fix activated before scroll completion");
  std::cout<<"PASS "<<checks<<" vertical Main Street fade samples and pre-arrival guard\n";
}
