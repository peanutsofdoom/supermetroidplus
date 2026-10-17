// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <vector>
#include <iostream>
using namespace SMPWide;
int main(){
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),regs(205);
 auto put=[](auto& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;};
 put(ram,0x998,8);put(ram,0x79b,0xa98d);put(ram,0x911,512);
 put(ram,0xf78,0xddbf);put(ram,0xf7a,512);put(ram,0xf7e,100);put(ram,0xf8e,0x8000);put(ram,0xfa6,0xa4);
 unsigned a=0x24*32768;put(rom,a,1);put(rom,a+2,0x8000);put(rom,a+5,0x2000);
 for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
 ram[0x370]=0;ram[0x371]=100;ram[0x570]=2;put(ram,0x372,0x2000);
 for(unsigned y=0;y<8;y++)v[y*2]=255;put(p,258,31);
 for(unsigned stage:{0x3eu,0x40u,0x42u,0x44u,0x46u,0x48u,0x52u}){
  put(ram,0xfa8,stage);EnemySampler e(ram,rom,v,p,regs);
  if(!e.pixel(-43,101) || !e.pixel(0,101) || e.pixel(0,101,true))throw std::runtime_error("Relocated actor/reference mismatch");
 }
 for(unsigned stage:{0u,0x45u,0x58u}){put(ram,0xfa8,stage);EnemySampler e(ram,rom,v,p,regs);if(e.pixel(-43,101))throw std::runtime_error("Other phase moved");}
 put(ram,0xfa8,0x40);put(ram,0x79b,0xa923);EnemySampler e(ram,rom,v,p,regs);if(e.pixel(-43,101))throw std::runtime_error("Other room moved");
 put(ram,0x79b,0xa98d);put(ram,0xfa8,0x52);ram[0x371]=240;
 for(unsigned x=299;x<=333;x++) {
   put(ram,0xf7a,512+x);EnemySampler skull(ram,rom,v,p,regs);
   if(!skull.pixel(int(x)-43,101))throw std::runtime_error("Skull culled before43px translation");
 }
 put(ram,0xf7a,480);ram[0x370]=224;ram[0x371]=100;ram[0x570]=3;
 EnemySampler offstage(ram,rom,v,p,regs);
 if(offstage.pixel(-32,101))throw std::runtime_error("Old offstage OAM copy left behind");
 std::cout<<"PASS whole-actor43px shift, original-OAM reference, phase and room guards\n";
}
