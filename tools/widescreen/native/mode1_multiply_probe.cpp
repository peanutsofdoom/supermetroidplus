// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <cassert>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(i*8);}
int main(){
 std::vector<uint8_t> ram(131072),v(65536),p(512),r(205);
 put(ram,0x998,8);put(ram,0x7a5,64);put(ram,0x7a7,64);
 for(unsigned y=0;y<8;y++)v[y*2]=255;put(p,2,992);
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority0,4);reg(r,Reg::hoffset,128);
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 auto row=[&](){return RowTimeline(scene,memoryImage(v,65536),memoryImage(p,512),r,40);};
 auto reference=row();reference.finish();auto result=row();auto after=r;
 for(unsigned i=0;i<24;i++){
  reg(after,i%3==2?Reg::m7b:Reg::m7a,0xff00+i);reg(after,Reg::latch_mode7,i);reg(after,Reg::ppu1_mdr,i+30);reg(after,Reg::latch_cgramAddress,i+27);reg(after,Reg::latch_oamAddress,512-i);
  result.registerWrite(i+1,100+i*20,i%3==2?0x211c:0x211b,after);
 }
 result.finish();assert(&result.at(0)==&result.at(255));
 for(int x=-43;x<299;x++){assert(result.pixel(x).valid);assert(result.pixel(x).rgb==reference.pixel(x).rgb);}
 // A later real brightness write still produces its own visible split.
 auto control=row();control.registerWrite(1,100,0x211b,after);reg(after,Reg::displayBrightness,8);control.registerWrite(2,700,0x2100,after);control.finish();
 assert(control.pixel(0).rgb==reference.pixel(0).rgb);assert(control.pixel(255).rgb!=reference.pixel(255).rgb);
 for(auto k:{Reg::displayBrightness,Reg::hoffset,Reg::bg2_aboveEnable,Reg::screen_colorRed,Reg::obj_tiledataAddress}){auto bad=r;reg(bad,k,Registers(r).get(k)+1);assert(!mode1MultiplyWrite(0x211b,Registers(r),Registers(bad)));}
 auto mode7=r;reg(mode7,Reg::bgMode,7);assert(!mode1MultiplyWrite(0x211b,Registers(mode7),Registers(mode7)));
 for(unsigned a:{0x211a,0x211d,0x2122})assert(!mode1MultiplyWrite(a,Registers(r),Registers(r)));
 bool rejected=false;try{auto q=row();q.memoryWrite({1,3,0,1,1});q.registerWrite(1,400,0x211b,r);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 rejected=false;try{auto q=row();q.registerWrite(2,400,0x211b,r);q.registerWrite(1,300,0x211c,r);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 std::cout<<"PASS Mode1 arithmetic leaves342pixels unchanged, no raster splits, real changes/order/memory/Mode7 remain guarded\n";
}
