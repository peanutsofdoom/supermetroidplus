#include "enemy_objects.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int run(int argc,char** argv){
 if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});if(rom.size()!=3145728)return 3;
 std::vector<uint8_t> ram(131072),v(65536),pal(512),regs(205);put(ram,0x998,8);
 for(unsigned t=0;t<2048;t++)for(unsigned y=0;y<8;y++)v[t*32+y*2]=255;
 for(unsigned p=0;p<8;p++)put(pal,(128+p*16+1)*2,992);
 unsigned cases=0,pixels=0;
 for(unsigned slot=10;slot<=18;slot+=2)for(int x:{-20,276})for(unsigned pointerAddress:{0x9fc1u,0x9fc9u,0x9fd1u,0x9fd9u,0xa219u,0xa21bu,0xa21du,0xa21fu,0xa221u,0xa223u}) {
  put(ram,0xc40+slot,1);put(ram,0xc18+slot,0x500);put(ram,0xb64+slot,uint16_t(x));put(ram,0xb78+slot,64);
  auto pointer=word(rom,0x13*32768+(pointerAddress&32767));put(ram,0xcb8+slot,pointer);
  EnemySampler sampler(ram,rom,v,pal,regs);unsigned count=0;
  for(int px=x-20;px<x+20;px++)if(px<0 || px>=256)for(unsigned y=32;y<96;y++)if(auto p=sampler.pixel(px,y)){if(p->color!=992 || p->order>=smpObjectGroup(1))throw std::runtime_error("bomb art/order");count++;}
  if(!count || sampler.pixel(128,64))throw std::runtime_error("missing bomb or touched native center");pixels+=count;cases++;
  put(ram,0xc18+slot,0x100);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("beam in bomb slot");
  put(ram,0xc18+slot,0x300);put(ram,0xc7c+slot,0);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("expired power seed");
  put(ram,0xc7c+slot,1);if(!EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("live power seed missing");
  put(ram,0xc40+slot,0);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("inactive bomb rendered");
 }
 std::cout<<"PASS "<<cases<<" real bomb/explosion cases, "<<pixels<<" margin pixels, all five slots, fuse/deletion and native-center guards\n";return 0;
}
int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}

