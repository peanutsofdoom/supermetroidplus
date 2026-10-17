#include "enemy_objects.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(int argc,char** argv){
  if(argc!=2)throw std::runtime_error("ROM path required");
  std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{});
  if(rom.size()!=3145728)throw std::runtime_error("ROM shape");
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x1a93,64);put(ram,0x19bb,0x400);
  for(unsigned tile:{0x1fu,0x0fu})for(unsigned y=0;y<8;y++)vram[8192+tile*32+y*2]=128;
  put(pal,(128+48+1)*2,992);
  unsigned cases=0,pixels=0;
  for(unsigned kind=0;kind<3;kind++) {
    unsigned ids[]={0x9db0,0x9dbe,0x9dcc},maps[]={0x8420,0x8427,0x842e},attrs[]={0x231f,0x230f,0x630f};
    unsigned map=0x0d*32768+(maps[kind]&32767);
    if(word(rom,map)!=1 || word(rom,map+5)!=attrs[kind])throw std::runtime_error("retail spritemap identity differs");
    put(ram,0x1997,ids[kind]);put(ram,0x1b6b,maps[kind]);
    int offset=kind?4:3;
    for(int center:{-20,280}) {
      put(ram,0x1a4b,uint16_t(center));EnemySampler sampler(ram,rom,vram,pal,regs);
      for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
        auto p=sampler.pixel(center-offset+x,64-offset+y+1);
        bool opaque=x==(kind==2?7:0);
        if(bool(p)!=opaque || (p && (p->color!=992 || p->palette!=3)))throw std::runtime_error("Mini Kraid edge art/flip/palette mismatch");
        pixels++;
      }
      if(sampler.pixel(128,64))throw std::runtime_error("native center touched");
      cases++;
    }
  }
  for(unsigned id:{0u,0x9ddau}){
    put(ram,0x1997,id);if(EnemySampler(ram,rom,vram,pal,regs).count())throw std::runtime_error("inactive/unknown projectile resurrected");
  }
  std::cout<<"PASS: "<<cases<<" real-ROM Mini Kraid edge cases, "<<pixels<<" art/flip/palette pixels and inactive/unknown guards\n";
}
