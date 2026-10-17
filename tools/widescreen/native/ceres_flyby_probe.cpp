#include "cinematic_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace SMPWide;
int main(){try{
 std::vector<uint8_t> v(65536),p(512),o(544),r(205);
 auto put=[](auto& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;};
 auto reg=[&](Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=n>>(8*i);};
 auto check=[](bool b,const char* why){if(!b)throw std::runtime_error(why);};
 reg(Reg::bgMode,7);reg(Reg::displayBrightness,15);reg(Reg::m7a,256);reg(Reg::m7d,256);reg(Reg::m7x,56);reg(Reg::m7y,24);reg(Reg::obj_aboveEnable,1);reg(Reg::obj_priority0,2);
 for(unsigned i=0;i<128;i++)o[i*4+1]=240;
 o[0]=240;o[1]=100;put(o,2,0x0809);for(unsigned y=0;y<8;y++)v[9*32+y*2]=255;put(p,(128+4*16+1)*2,31);
 auto pixel=[&](int x,unsigned phase=0xa38f,unsigned scene=2){return CinematicRow(v,p,o,r,101,phase,scene).pixel(x);};
 check(pixel(-16)==0xf80000 && pixel(240)==0xf80000,"star field did not extend or native star changed");
 put(o,2,0x0e09);check(pixel(-16)==0,"planet duplicated");put(o,2,0x089a);check(pixel(-16)==0,"caption duplicated");put(o,2,0x0809);check(pixel(-16,0xa38f,1)==0,"Ceres star rule leaked into ending");
 reg(Reg::obj_aboveEnable,0);reg(Reg::aboveEnable,1);reg(Reg::priority0,1);std::fill(v.begin(),v.end(),0);for(unsigned n=0;n<64;n++)v[n*2+1]=1;put(p,2,992);
 check(pixel(-1)==0 && pixel(0)==0x00f800,"parked ship exposed early or native center altered");
 check(pixel(-1,0xc9f9)==0x00f800,"flying ship margin clipped");reg(Reg::m7x,52);check(pixel(-1)==0x00f800,"station backdrop affected by ship staging");
 std::cout<<"PASS extended Ceres star field, single planet/caption, native-center preservation and parked/flying ship staging guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
