#include "enemy_objects.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){
  std::vector<uint8_t> ram(131072),rom(3145728),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);unsigned a=0xf78,enemy=0x22*32768+0x1000,shot=0x13*32768+0x1000;
  put(ram,a,0xc000);put(ram,a+6,60);put(ram,a+10,8);put(ram,a+22,0x9000);
  put(ram,a+30,0x200);put(ram,a+34,2);put(ram,a+46,0xa2);
  put(rom,enemy,1);put(rom,enemy+2,0x8000);
  put(ram,0xc40,1);put(ram,0xcb8,0x9000);put(ram,0xb78,60);
  put(rom,shot,1);put(rom,shot+2,0x8000);put(rom,shot+5,0x400);
  for(unsigned tile:{0u,1u,16u,17u})for(unsigned y=0;y<8;y++)vram[tile*32+y*2]=255;
  put(pal,(128+16+1)*2,31);put(pal,(128+32+1)*2,992);
  unsigned pixels=0;
  for(int origin:{-8,248}) {
    put(ram,a+2,uint16_t(origin));put(ram,0xb64,uint16_t(origin));
    ram[0x370]=uint8_t(origin);ram[0x371]=60;put(ram,0x372,0x200);ram[0x570]=origin<0?3:2;
    for(unsigned type:{0x100u,0x200u,0x700u,0x800u}) {
      put(ram,0xc18,type);EnemySampler sampler(ram,rom,vram,pal,regs);
      for(int x=origin;x<origin+16;x++)if(x<0 || x>=256) {
        auto p=sampler.pixel(x,61);
        // Native draw order puts flying missiles behind layer2 enemies,
        // but impact explosions before every enemy drawing layer.
        if(!p || p->color!=(type>=0x300?992u:31u))throw std::runtime_error("impact seam ordering differs");
        if(type>=0x300 && p->order>=smpEnemyOrder(2,0,0))throw std::runtime_error("impact must precede ship too");
        pixels++;
      }
      if(sampler.pixel(128,61))throw std::runtime_error("native center touched");
    }
  }
  std::cout<<"PASS: "<<pixels<<" overlapping missile/super/explosion edge pixels, ship ordering and native-center exclusion\n";
}
