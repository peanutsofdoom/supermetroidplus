// Passive Draygon body extension, exercised through actual row composition.
#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
 put(ram,0x998,8);put(ram,0x79b,0xda60);put(ram,0xf78,0xde3f);
 put(ram,0x7a5,64);put(ram,0x7a7,32);put(ram,0x911,100);put(ram,0x915,256);
 put(ram,0xf7a,20);put(ram,0xf7e,356);
 for(unsigned y=0;y<8;y++)vram[32+y*2]=255;
 for(unsigned i=0;i<2048;i++)put(vram,0x9000+i*2,1);
 put(pal,2,31);reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);
 reg(regs,Reg::hoffset,100);reg(regs,Reg::voffset,256);
 reg(regs,Reg::bg2_screenAddress,0x4800);reg(regs,Reg::bg2_screenSize,1);reg(regs,Reg::bg2_priority0,4);
 auto color=[&](int x,unsigned line){auto o=std::make_shared<const OwnerImage>(1,ram);SceneSelection s{o,o,{},SceneKind::Ordinary};RowTimeline r(s,memoryImage(vram,65536),memoryImage(pal,512),regs,line);r.finish();return r.pixel(x);};
 auto require=[&](int x,unsigned line,bool red){auto p=color(x,line);if(!p.valid || p.rgb!=(red?std::array<uint8_t,3>{248,0,0}:std::array<uint8_t,3>{0,0,0}))throw std::runtime_error("Draygon row composition mismatch");};
 // Native body disabled at dx=-80: margin recovers its actual BG2 tile/palette,
 // while every native-center sample retains the disabled-plane result.
 require(-20,100,true);require(100,100,false);
 put(ram,0xf7a,440);require(280,100,true);require(255,100,false);
 put(ram,0xf7a,464);require(280,100,false);
 put(ram,0xf7a,20);put(ram,0xf7e,276);require(-20,95,true);require(-20,96,false);
 put(ram,0xf7e,466);require(-20,127,false);require(-20,128,true);
 put(ram,0xf7e,356);put(ram,0xf86,0x200);require(-20,100,false);put(ram,0xf86,0);
 put(ram,0x79b,0xd78f);require(-20,100,false);put(ram,0x79b,0xda60);
 reg(regs,Reg::bg2_screenAddress,0x5000);require(-20,100,false);reg(regs,Reg::bg2_screenAddress,0x4800);
 // An already enabled native plane remains the reference for ordinary drawing.
 put(ram,0xf7a,100);reg(regs,Reg::bg2_aboveEnable,1);require(100,100,true);require(-20,100,true);
 std::cout<<"PASS Draygon disabled-body recovery, native-center preservation, vertical bands, native enabled plane and scope guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
