// SPDX-License-Identifier: GPL-3.0-or-later
#include "cinematic_scene.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 std::vector<uint8_t> rom(3145728),ram(131072);for(unsigned i=0;i<40;i++){put(rom,0x5e9cf+i*8+4,6);put(ram,0xe0c+i*16,0x8000|i);}
 put(ram,0x998,39);put(ram,0x1a05,1);CinematicStars stars;stars.configure(rom);
 unsigned a=0xe0c;put(ram,a,0x600);put(ram,a+2,254);put(ram,a+6,128);put(ram,a+10,6);put(ram,a+12,512);stars.observe(ram);
 check(stars.objects().empty(),"star duplicated before native exit");
 put(ram,a,0);put(ram,a+2,128);put(ram,a+6,128);put(ram,a+12,0);stars.observe(ram);
 check(stars.objects().size()==1 && stars.objects()[0].x==252 && stars.objects()[0].attr==0x9f2,"right star did not continue outward");
 stars.observe(ram);check(stars.objects().size()==1 && stars.objects()[0].x==254,"star tail moved inward");
 for(unsigned i=0;i<30;i++)stars.observe(ram);check(stars.objects().empty(),"tail survived outside wide area");
 stars.reset();put(ram,a,0x600);put(ram,a+2,5);put(ram,a+12,0xfe00);stars.observe(ram);put(ram,a,0);put(ram,a+2,128);put(ram,a+12,0);stars.observe(ram);
 check(stars.objects().size()==1 && stars.objects()[0].x==-1,"left star did not continue outward");
 put(ram,0x998,8);stars.observe(ram);check(stars.objects().empty(),"star tails leaked into gameplay");
 std::vector<uint8_t> v(65536),p(512),o(544),r(205);for(unsigned i=0;i<128;i++)o[i*4+1]=240;
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::obj_aboveEnable,1);reg(r,Reg::obj_priority0,3);reg(r,Reg::obj_tiledataAddress,0x6000);
 put(p,386,31);for(unsigned y=0;y<8;y++)v[0xfe00+y*2]=255;
 o[0]=5;o[1]=100;put(o,2,0x9f0);
 CinematicRow native(v,p,o,r,101,0xe7ba,1);check(native.pixel(5)==0xf80000 && native.pixel(261)==0,"native shooting star wrapped to opposite edge");
 CinematicStar tail{256,100,0x9f0};CinematicRow continued(v,p,o,r,101,0xe7ba,1,{},std::span(&tail,1));
 check(continued.pixel(256)==0xf80000 && continued.pixel(128)==0,"continued tail overlaps native center");
 reg(r,Reg::obj_tiledataAddress,0x4000);o[0]=0;put(o,2,0xeed);put(p,482,31);for(unsigned y=0;y<8;y++)v[0x8000+0xed*32+y*2]=255;
 CinematicRow purple(v,p,o,r,101,0xda1a,1);check(purple.pixel(256)==0xf80000,"purple field missing in margin");
 std::cout<<"PASS outward star tails, native-center exclusion, no mirror/wrap and purple-star margins\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
