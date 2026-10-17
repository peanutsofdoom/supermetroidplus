// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_state.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
int main(int argc,char** argv){
 if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});
 std::vector<uint8_t> r(131072),v(65536),p(512),fake(3145728);
 auto put=[](auto& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;};
 unsigned cases=0;
 for(unsigned angle:{10u,32u,64u,96u,118u,138u,160u,192u,224u,246u})for(unsigned width:{0u,1u,5u,10u}) {
  XrayCone c{128,120,angle,width};
  for(int y=0;y<224;y++){auto e=c.edges(y,rom);if(e.second>=e.first && (e.first< -1000000 || e.second>1000000))throw std::runtime_error("Unbounded cone");cases++;}
 }
 auto right=XrayCone{128,120,64,10}.edges(119,rom),left=XrayCone{128,120,192,10}.edges(119,rom);
 if(right.first!=128 || right.second<299 || left.first> -43 || left.second!=128)throw std::runtime_error("Both margin cones");
 put(r,0x7a5,32);put(r,0x7a7,16);put(r,0x7bb,0x8000);
 auto rw=[&](unsigned bank,unsigned a,unsigned n){put(fake,xrayRomAddress(bank,a),n);};
 rw(0x91,0xd2d6,0x5000);rw(0x91,0xd2d8,0xd310);rw(0x91,0xd2da,0xb000);rw(0x91,0xd2dc,0xd300);
 rw(0x91,0xd2de,0xf000);rw(0x91,0xd2e0,0xd320);rw(0x91,0xd2e2,0xffff);
 for(auto q:{std::pair{0xd300,0xd400},std::pair{0xd310,0xd410},std::pair{0xd320,0xd420}}){rw(0x91,q.first,0xff00);rw(0x91,q.first+2,q.second);}
 rw(0x91,0xd400,0xcf36);rw(0x91,0xd402,42);rw(0x91,0xd410,0xcebb);rw(0x91,0xd420,0xcf6f);
 for(unsigned i=0;i<4;i++){rw(0x91,0xd422+i*2,42+i);for(unsigned q=0;q<4;q++)put(r,0xa000+(42+i)*8+q*2,0x200+i*4+q);}
 put(r,0x10002+(3*32+5)*2,0xb001);put(r,0x10002+(3*32+4)*2,0x5001);r[0x16402+3*32+4]=1;
 put(r,0x10002+(5*32+6)*2,0xf001);
 XrayTerrain map(r,fake,v,p,0);
 if(map.entry(80,48)!=0x200 || map.entry(64,48)!=0x200 || map.entry(112,96)!=0x20c)throw std::runtime_error("Hidden/extension/compound block");
 put(r,0x1c37,0xdf89);put(r,0x1c87,(3*32+5)*2);rw(0x84,0x839d,0x9000);rw(0x84,0x9002,43);
 if(map.entry(80,48)!=0x204)throw std::runtime_error("Item PLM overlay");
 rw(0x8f,0x8010,0xa000);rw(0x8f,0xa000,5);rw(0x8f,0xa002,3);rw(0x8f,0xa004,44);
 if(map.entry(80,48)!=0x208)throw std::runtime_error("Room special overlay");
 rw(0x8f,0xa004,44|0x800);
 if(map.entry(80,48)!=0x20a)throw std::runtime_error("Native special vertical row swap");
 if(xrayOwned(r))throw std::runtime_error("Inactive ownership");put(r,0xa78,1);put(r,0x18b4,1);put(r,0x18fc,0x88);put(r,0x18f0,0x86ef);
 if(!xrayOwned(r))throw std::runtime_error("X-ray ownership");put(r,0x18f0,0x90df);if(xrayOwned(r))throw std::runtime_error("Power Bomb confused with X-ray");
 std::cout<<"PASS "<<cases<<" cone rows, both margins, reveal/extension/compound/item/special maps and ownership guards\n";
}
