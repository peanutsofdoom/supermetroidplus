#include "enemy_objects.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{});
  if(rom.size()!=3145728)return 3;
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x1997,0xa17b);put(ram,0x1bd7,0x1000);put(ram,0x1a93,64);
  for(unsigned tile:{0x6fu,0x53u})for(unsigned y=0;y<8;y++)vram[tile*32+y*2]=128;
  put(pal,(128+5*16+1)*2,992);unsigned pixels=0;
  for(unsigned map:{0x8bb5u,0x8c29u})for(int center:{-20,280}) {
    unsigned address=0x0d*32768+(map&32767);if(word(rom,address)!=1)throw std::runtime_error("laser map shape");
    put(ram,0x1b6b,map);put(ram,0x1a4b,uint16_t(center));EnemySampler sampler(ram,rom,vram,pal,regs);
    for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
      auto p=sampler.pixel(center-4+x,61+y);
      if(bool(p)!=(x==0) || (p && (p->color!=992 || p->order>=smpObjectGroup(2))))throw std::runtime_error("laser margin art/order mismatch");
      pixels++;
    }
  }
  put(ram,0x1997,0);if(EnemySampler(ram,rom,vram,pal,regs).count())throw std::runtime_error("deleted laser drawn");
  std::cout<<"PASS: "<<pixels<<" real-map laser/muzzle pixels at both margins, high draw group and deletion\n";
}
