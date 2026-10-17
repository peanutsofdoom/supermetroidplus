// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include "cinematic_scene.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 auto check=[](bool v,const char* why){if(!v)throw std::runtime_error(why);};
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),r(205),oam(544);
 put(ram,0x998,8);put(ram,0x79b,0xdd58);put(ram,0x7bb,0xdd6e);put(ram,0x7a5,64);put(ram,0x7a7,16);put(ram,0xf78,0xec7f);put(ram,0xfb8,0xec3f);put(ram,0x10002+(6*64+20)*2,0x8000);
 for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
 for(unsigned y=0;y<8;y++){v[32+y*2]=255;v[96+y*2]=255;v[96+y*2+1]=255;}
 put(p,482,31);put(p,6,31744);reg(r,Reg::obj_priority3,8);
 ram[0x370]=240;ram[0x371]=100;put(ram,0x372,0x3e01);
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority1,15);reg(r,Reg::screenAddress,0x5000);reg(r,Reg::obj_aboveEnable,1);put(v,(0x5000+12*32+30)*2,0x2003);
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));row.finish();
 check(row.centerCandidate(240),"native arena sprite blocks wall recomposition");
 check(row.pixel(240).rgb==std::array<uint8_t,3>{0,0,248},"native old-wall witness wrong");
 check(row.pixel(240,true).rgb==std::array<uint8_t,3>{248,0,0},"Samus/Metroid still hidden by old wall");
 ram[0x370]=24;ram[0x570]=1;
 auto sample=[&](){return EnemySampler(ram,rom,v,p,r).pixel(280,101);};
 check(sample() && sample()->color==31,"native custom margin sprite missing");
 put(ram,0x7bb,0);check(!sample(),"arena state guard leaked");put(ram,0x7bb,0xdd6e);
 unsigned baby=0xff8,mapBaby=0x149000;put(ram,baby,0xecbf);put(ram,baby+2,280);put(ram,baby+6,100);put(ram,baby+22,0x9000);put(ram,baby+46,0xa9);put(ram,baby+48,0xc7cc);
 put(rom,mapBaby,1);put(rom,mapBaby+2,0);rom[mapBaby+4]=0;put(rom,mapBaby+5,0x3e01);
 check(!sample(),"parked Metroid revealed before scripted swoop");
 put(ram,baby+48,0xc7ec);check(sample() && sample()->color==31,"Metroid stayed hidden during swoop");put(ram,baby,0);
 // Bombs emit three separate afterburn projectile kinds through native86:C52D.
 ram[0x371]=240;ram[0x570]=0;unsigned map=(0x8d&127)*32768;
 put(rom,map,1);put(rom,map+2,0);rom[map+4]=0;put(rom,map+5,0x3e01);
 put(ram,0x1a4b,280);put(ram,0x1a93,100);put(ram,0x1b6b,0x8000);
 for(unsigned id:{0x9650u,0x966cu,0x967au}){put(ram,0x1997,id);check(sample() && sample()->color==31,"bomb flame piece missing beyond256");}
 put(ram,0x79b,0x92fd);check(!sample(),"bomb flame exception escaped arena");
 // A single authored ending explosion must not repeat in either margin.
 ram.assign(131072,0);v.assign(65536,0);p.assign(512,0);r.assign(205,0);
 for(unsigned i=0;i<128;i++)oam[i*4+1]=240;
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority0,2);reg(r,Reg::tiledataAddress,0x4000);reg(r,Reg::screenAddress,0x7400);reg(r,Reg::bg2_screenAddress,0x7800);
 put(p,2,31);for(unsigned y=0;y<8;y++)v[0x8020+y*2]=255;
 for(unsigned i=0;i<1024;i++)put(v,(0x7400+i)*2,1);
 CinematicRow explosion(v,p,oam,r,101,0xdb9d,1);
 check(explosion.pixel(128)==0xf80000,"native explosion changed");
 check(explosion.pixel(-1)==0 && explosion.pixel(256)==0,"ending explosion repeated");
 CinematicRow other(v,p,oam,r,101,0xdb9d,2);check(other.pixel(-1)==0xf80000,"ending explosion scope leaked into intro");
 // The first flash reaches the edges. Continue each edge independently,
 // without wrapping interior art or altering a single native-center sample.
 reg(r,Reg::screenAddress,0x7000);reg(r,Reg::bg2_screenAddress,0x7c00);
 put(p,4,992);put(p,6,31744);
 for(unsigned y=0;y<8;y++){v[0x8040+y*2+1]=255;v[0x8060+y*2]=255;v[0x8060+y*2+1]=255;}
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)put(v,(0x7000+y*32+x)*2,x==0?1:x==31?3:2);
 CinematicRow flash(v,p,oam,r,101,0xdb9d,1);
 check(flash.pixel(128)==0x00f800,"flash center changed");
 for(int x=-43;x<0;x++)check(flash.pixel(x)==flash.pixel(0) && flash.pixel(x)==0xf80000,"left flash edge wrapped or disappeared");
 for(int x=256;x<299;x++)check(flash.pixel(x)==flash.pixel(255) && flash.pixel(x)==0x0000f8,"right flash edge wrapped or disappeared");
 reg(r,Reg::screenSize,1);CinematicRow unrelatedMap(v,p,oam,r,101,0xdb9d,1);check(unrelatedMap.pixel(-1)==0,"large-map scope leaked");
 std::cout<<"PASS Mother Brain wall/OBJ witness, custom native margin sprites, bomb afterburn maps and single ending explosion\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
