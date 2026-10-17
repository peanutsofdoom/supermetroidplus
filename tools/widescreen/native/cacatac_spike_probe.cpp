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
 for(unsigned slot=0;slot<=34;slot+=2)for(int x:{-20,276})for(unsigned a:{0xd930u,0xd936u,0xd93cu,0xd942u,0xd948u,0xd94eu,0xd954u,0xd95au,0xd960u,0xd966u}){
  put(ram,0x1997+slot,0xdafe);put(ram,0x1a03+slot,0xd9db);put(ram,0x1bd7+slot,0x7000);put(ram,0x1a4b+slot,uint16_t(x));put(ram,0x1a93+slot,64);
  put(ram,0x1b6b+slot,word(rom,0x6*32768+(a&32767)));EnemySampler sampler(ram,rom,v,pal,regs);unsigned count=0;
  for(int px=x-32;px<x+32;px++)if(px<0 || px>=256)for(unsigned y=32;y<96;y++)if(auto p=sampler.pixel(px,y)){if(p->color!=992 || p->order<smpObjectGroup(1) || p->order>=smpObjectGroup(2))throw std::runtime_error("spike art/order");count++;}
  if(!count || sampler.pixel(128,64))throw std::runtime_error("missing spike/native exclusion");pixels+=count;cases++;
  put(ram,0x1997+slot,0x1234);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("unrelated projectile expanded");
  put(ram,0x1997+slot,0xdafe);put(ram,0x1a03+slot,0xefe0);if(!EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("pickup transition disappeared");
  put(ram,0x1997+slot,0);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("deleted effect drawn");
 }
 std::cout<<"PASS "<<cases<<" real ten-direction Cacatac spike cases, "<<pixels<<" pixels, all18slots, both margins, ordering and pickup/deletion guards\n";return 0;
}
int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
