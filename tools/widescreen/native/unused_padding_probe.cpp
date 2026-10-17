#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 std::vector<uint8_t> ram(131072),v(65536),p(512),r(205);
 put(ram,0x998,8);put(ram,0x79b,0xd0b9);put(ram,0x7bb,0xd0c6);put(ram,0x7a5,64);put(ram,0x7a7,16);
 for(unsigned i=0;i<64*16;i++)put(ram,0x10002+i*2,0x83ff);
 for(unsigned i=0;i<4;i++)put(ram,0xa000+1023*8+i*2,1023);
 for(unsigned i=0;i<32;i++)v[1023*32+i]=255;
 put(p,30,32767);reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority0,1);
 auto sample=[&](unsigned game,unsigned state,int x){put(ram,0x998,game);put(ram,0x7bb,state);SceneGeometry g(ram,ram,{},v,p,0,false,0,0);Mode1 row(g,v,p,r,100,128,0);return row.pixel(x);};
 for(auto room:{std::pair{0xd0b9u,0xd0c6u},std::pair{0xcfc9u,0xcfd6u},std::pair{0xd340u,0xd34du},std::pair{0xb6eeu,0xb6fbu}}){
 put(ram,0x79b,room.first);
 for(unsigned game:{8u,9u,11u})for(int x=-43;x<299;x++){
  auto a=sample(game,room.second,x);auto expected=x<0||x>=256?std::array<uint8_t,3>{0,0,0}:std::array<uint8_t,3>{248,248,248};
  if(!a.valid || a.rgb!=expected)throw std::runtime_error("Padding or native center mismatch");
 }
 for(int x:{-43,298}){
  if(sample(8,room.second+1,x).rgb!=std::array<uint8_t,3>{248,248,248})throw std::runtime_error("Unqualified room changed");
  if(sample(21,room.second,x).rgb!=std::array<uint8_t,3>{248,248,248})throw std::runtime_error("Death flash clipped");
 }
 }
 // A room header can change before its assets. The retained departure still
 // owns filler cells; during scrolling the selected side supplies the cells.
 auto prior=ram,view=ram;
 put(prior,0x79b,0xcfc9);put(prior,0x7bb,0xcfd6);put(prior,0x78d,0xa3c0);
 put(ram,0x79b,0xd0b9);put(ram,0x7bb,0xd0c6);put(ram,0x78d,0xa3c0);
 put(view,0x79b,0xd0b9);put(view,0x7bb,0xd0c6);put(view,0x998,11);put(view,0x78d,0xa3c0);
 put(view,0x791,0);put(view,0x911,65500);put(view,0x927,0);put(view,0x929,0);
 SceneGeometry joined(ram,view,prior,v,p,0,true,256,0);
 if(!joined.unusedPadding(-20,100) || !joined.unusedPadding(20,100))throw std::runtime_error("Transition lost selected padding");
 put(prior,0x10002+(6*64+30)*2,0x8001);
 SceneGeometry authored(ram,view,prior,v,p,0,true,256,0);
 if(authored.unusedPadding(-20,100))throw std::runtime_error("Authored departing terrain hidden");
 std::cout<<"PASS padding margins, native center, room-state and death guards\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
