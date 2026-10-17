// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_objects.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){try{
 std::vector<uint8_t> ram(131072),maps(0x272),oam(544),v(65536),p(512),regs(205);
 put(ram,0x79b,0x91f8);put(maps,0,1);put(maps,5,1);
 for(unsigned i=0;i<128;i++)oam[i*4+1]=240;
 for(unsigned i=0;i<3;i++) {
  unsigned a=0xf78+i*64;put(ram,a,i?0xd0bf:0xd07f);put(ram,a+2,128);put(ram,a+6,100+i*16);put(ram,a+10,64);put(ram,a+14,0x2400);put(ram,a+22,0xad81);put(ram,a+32,i);put(ram,a+34,2);put(ram,a+46,0xa2);
  oam[i*4]=128;put(oam,i*4+2,1+i);
 }
 auto count=[&](){return ShipSampler(ram,maps,oam,v,p,regs,ram).descriptors().size();};
 if(count()!=0)throw std::runtime_error("native hidden ship resurrected");
 for(unsigned i=0;i<3;i++)oam[i*4+1]=100+i*16;
 if(count()!=3)throw std::runtime_error("visible ship lost");
 oam[5]=240;if(count()!=2)throw std::runtime_error("hidden group must remain absent");
 oam[0]=127;bool rejected=false;try{count();}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("unwitnessed ship accepted");
 oam[0]=128;for(unsigned i=0;i<3;i++)oam[i*4+1]=240;put(ram,0xf86,0);
 rejected=false;try{count();}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("hidden group bypassed scene guard");
 std::cout<<"PASS ordered native hidden ship groups, mixed visibility and mismatch guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
