#include "scene_geometry.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace SMPWide;
int main(){try {
 std::vector<uint8_t> ram(131072),prior(131072),vram(65536),palette(512);
 auto put=[](auto& b,unsigned a,unsigned n){b[a]=n;b[a+1]=n>>8;};
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 put(ram,0x79b,0x9c5e);put(ram,0x7bb,0x9c6b);put(ram,0x7a5,48);put(ram,0x7a7,32);put(ram,0x921,0xfd00);put(ram,0x917,442);
 for(unsigned q=0;q<4;q++){put(ram,0xa008+q*2,1);put(ram,0xa010+q*2,2);}
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<48;x++)put(ram,0x19602+(y*48+x)*2,x<28?1:2);
 for(unsigned y=0;y<8;y++){vram[32+y*2]=255;vram[64+y*2+1]=255;vram[96+y*2]=vram[96+y*2+1]=255;}
 put(palette,2,992);put(palette,4,31);put(palette,6,31744);
 unsigned cases=0;
 for(unsigned game:{8u,9u,11u})for(unsigned phase:{0xe29eu,0xe2dbu,0xe2f7u,0xe737u})for(unsigned ring:{1u,2u,3u}){
  put(ram,0x998,game);put(ram,0x99c,phase);for(unsigned n=0;n<2048;n++)put(vram,0x9000+n*2,ring);
  SceneGeometry g(ram,ram,{},vram,palette,0,false,0,0);
  for(int x:{-43,-1,256,298}){auto p=g.background(x,100,unsigned(0xfd00+442)&65535,0,0,442,0);check(p && p->valid && p->color==unsigned(x+442<448?992:31),"background depends on stale ring or wrong wrapped scroll");cases++;}
 }
 prior=ram;put(prior,0x79b,0x9ad9);put(prior,0x7bb,0x9ae6);put(prior,0x78d,0x8cca);
 put(ram,0x998,11);put(ram,0x78d,0x8cca);put(ram,0x99c,0xe6a2);put(ram,0x929,0);
 for(unsigned direction:{0u,1u}){
  put(ram,0x791,direction);put(ram,0x927,direction?512:442);put(ram,0x911,direction?512:442);
  SceneGeometry arrived(ram,ram,prior,vram,palette,0,true,0,0);
  check(arrived.background(-43,100,unsigned(0xfd00+442)&65535,0,0,442,0).has_value(),"arrival fade lost full-room background");
  put(ram,0x911,direction?600:400);SceneGeometry crossing(ram,ram,prior,vram,palette,0,true,0,0);
  check(!crossing.background(-43,100,unsigned(0xfd00+442)&65535,0,0,442,0),"background took ownership before native arrival");
 }
 auto wrong=ram;put(wrong,0x79b,0x9ad9);put(wrong,0x7bb,0x9ae6);
 SceneGeometry newHeader(ram,wrong,{},vram,palette,0,false,0,0);check(!newHeader.background(-43,100,442,0,0,442,0),"old room retained background after header switch");
 put(ram,0x998,8);put(ram,0x7bb,0x9c6c);SceneGeometry foreign(ram,ram,{},vram,palette,0,false,0,0);check(!foreign.background(-43,100,442,0,0,442,0),"foreign state changed");
 std::cout<<"PASS "<<cases<<" Fireflea margin samples, stale-ring independence, wrapped scroll, entry/departure fades and ownership guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
