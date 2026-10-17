#include "cinematic_scene.hpp"
#include "ceres_cinematic_wall.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=n>>(8*i);}
int main(){try{
 auto check=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),o(544),r(205);
 put(ram,0x998,8);put(ram,0x79b,0xe0b5);put(ram,0x7bb,0xe0c7);put(ram,0x79f,6);
 put(ram,0x7a5,32);put(ram,0x7a7,16);put(ram,0x7a9,2);put(ram,0xf78,0xe13f);put(ram,0x10002+(6*32+20)*2,0x8000);
 unsigned a=0x1038,base=0x26*32768+0x7b2f;
 put(ram,a,0xe23f);put(ram,a+2,328);put(ram,a+6,127);put(ram,a+60,6);put(ram,a+22,0xfb2f);put(ram,a+30,0xe00);
 put(rom,base,13);
 for(unsigned i=0;i<13;i++){put(rom,base+2+i*5,0x81f8);rom[base+4+i*5]=128+i*16;put(rom,base+5+i*5,0x4e6);}
 CeresCinematicWall wall;wall.configure(rom);wall.observe(ram);
 check(wall.objects().size()==13,"relocated live wall missing");check(wall.objects()[0].x==320 && wall.objects()[0].y==-1,"signed map coordinates wrong");check(wall.objects()[0].attr==0xee6,"live enemy palette lost");
 put(ram,a+14,0x100);wall.observe(ram);check(wall.objects().empty(),"invisible wall shown");put(ram,a+14,0);
 put(ram,a+2,248);wall.observe(ram);check(wall.objects().empty(),"unrelocated wall duplicated");put(ram,a+2,328);
 put(ram,a+22,0xface);wall.observe(ram);check(wall.objects().empty(),"wrong map accepted");put(ram,a+22,0xfb2f);wall.observe(ram);
 for(unsigned i=0;i<128;i++)o[i*4+1]=240;
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::bg2_tiledataAddress,0x6000);reg(r,Reg::bg2_screenAddress,0x4800);reg(r,Reg::bg2_screenSize,1);reg(r,Reg::bg2_hoffset,256);reg(r,Reg::bg2_aboveEnable,1);reg(r,Reg::bg2_priority0,1);
 put(p,2,31);for(unsigned y=0;y<8;y++)v[0xc000+32+y*2]=255;
 for(unsigned row=26;row<28;row++)for(unsigned col=28;col<30;col++)put(v,(0x4800+1024+row*32+col)*2,1);
 for(unsigned line:{208u,224u})for(int x=256;x<336;x++)check(CinematicRow(v,p,o,r,line,0,4).pixel(x)==CinematicRow(v,p,o,r,line,0,4).pixel(224+(x-256)%16),"BG2 floor extension wrong");
 check(CinematicRow(v,p,o,r,208,0,4).pixel(256)!=0,"floor suppressed as a caption");check(CinematicRow(v,p,o,r,208,0,4).pixel(336)==0,"room boundary leaked");
 check(CinematicRow(v,p,o,r,208,0,3).pixel(256)==0,"unrelated Ceres scene changed");
 reg(r,Reg::bgMode,7);reg(r,Reg::bg2_aboveEnable,0);reg(r,Reg::obj_tiledataAddress,0x6000);reg(r,Reg::obj_aboveEnable,1);reg(r,Reg::obj_priority0,2);
 put(p,(128+7*16+1)*2,31<<10);for(unsigned y=0;y<8;y++)v[0xc000+0xe6*32+y*2]=255;
 check(CinematicRow(v,p,o,r,1,0,4,{},wall.objects()).pixel(320)==248,"wall pixel did not use native palette");
 std::cout<<"PASS Ceres Mode7 wall coordinates, visibility/map/position/palette guards, floor IRQ BG2 and scene boundaries\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
