#include "enemy_objects.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){try{
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),pal(512),regs(205);put(ram,0x998,8);
 unsigned map=0x28*32768+0x1000;put(rom,map,1);put(rom,map+2,0x8000);
 for(unsigned slot:{0u,64u}){unsigned a=0xf78+slot;put(ram,a,0xeabf);put(ram,a+2,uint16_t(slot?-12:-20));put(ram,a+6,60);put(ram,a+10,slot?16:8);put(ram,a+22,0x9000);put(ram,a+30,slot?0x400:0x200);put(ram,a+34,5);put(ram,a+46,0xa8);}
 for(unsigned t:{0u,1u,16u,17u})for(unsigned y=0;y<8;y++)v[t*32+y*2]=255;
 put(pal,(128+16+1)*2,31);put(pal,(128+32+1)*2,992);
 ram[0x370]=244;ram[0x371]=60;put(ram,0x372,0x400);ram[0x570]=3;
 EnemySampler sampler(ram,rom,v,pal,regs);
 for(int x=-20;x<0;x++){auto p=sampler.pixel(x,61);if(!p || p->color!=(x< -12?31u:992u))throw std::runtime_error("offscreen-only wing replaced native overlap");}
 if(sampler.pixel(0,61))throw std::runtime_error("native center changed");
 put(ram,0xf78+64+14,0x100);auto p=EnemySampler(ram,rom,v,pal,regs).pixel(-10,61);if(!p || p->color!=31)throw std::runtime_error("invisible enemy resurrected");
 std::cout<<"PASS native-overlap priority, offscreen extension, center exclusion and suppression\n";
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
