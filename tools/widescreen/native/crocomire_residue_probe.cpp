// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <vector>
#include <iostream>
using namespace SMPWide;
int main(){
 std::vector<uint8_t> r(131072),v(65536),p(512),regs(205);
 auto put=[](auto& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;};
 auto reg=[&](Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)regs[f.offset+i]=n>>(8*i);};
 put(r,0x79b,0xa98d);put(r,0xf78,0xddbf);put(r,0xfa8,0x40);put(r,0x998,8);put(r,0x7a5,128);put(r,0x7a7,16);r[0x59]=0x49;
 reg(Reg::bgMode,1);reg(Reg::displayBrightness,15);reg(Reg::bg2_aboveEnable,1);reg(Reg::bg2_priority0,2);reg(Reg::bg2_screenAddress,0x4800);
 for(unsigned y=0;y<8;y++)v[y*2]=255;put(p,2,0x4210);
 SceneGeometry g(r,r,{},v,p,0,false,0,0);Mode1 m(g,v,p,regs,100,512,0);
 auto native=m.pixel(100),fixed=m.pixel(100,true);
 if(!m.stagingCandidate(100) || !native.valid || !fixed.valid || native.rgb!=std::array<uint8_t,3>{128,128,128} || fixed.rgb!=std::array<uint8_t,3>{0,0,0})throw std::runtime_error("Old boss BG2 residue not witnessed/removed");
 put(r,0x79b,0xa923);SceneGeometry other(r,r,{},v,p,0,false,0,0);Mode1 normal(other,v,p,regs,100,512,0);
 if(normal.stagingCandidate(100) || normal.pixel(100).rgb!=normal.pixel(100,true).rgb)throw std::runtime_error("Other room changed");
 std::cout<<"PASS native boss-BG2 reference, residue cleanup and other-room guard\n";
}
