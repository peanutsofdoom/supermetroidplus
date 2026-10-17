#include "enemy_objects.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void require(bool b){if(!b)throw std::runtime_error("enemy oracle mismatch");}
int main(){
  std::vector<uint8_t> ram(131072),rom(3145728),vram(65536),pal(512),regs(205);
  unsigned a=0xf78,pos=0x22*32768+0x1000;
  put(ram,0x998,8);put(ram,a,0xc000);put(ram,a+2,65526);put(ram,a+6,60);
  put(ram,a+10,8);put(ram,a+22,0x9000);put(ram,a+30,0x200);put(ram,a+46,0xa2);
  put(rom,pos,1);put(rom,pos+2,0x8000);
  for(unsigned y=0;y<8;y++)vram[y*2]=255;
  put(pal,(128+16+1)*2,31);put(pal,(128+96+1)*2,992);
  auto sample=[&](int x){return EnemySampler(ram,rom,vram,pal,regs).pixel(x,61);};
  require(sample(-10) && sample(-10)->color==31);require(!sample(0));
  ram[a+47]=8;require(sample(-10) && sample(-10)->color==31);ram[a+47]=0;
  put(ram,0x998,0x12);require(sample(-10) && sample(-10)->color==31);
  put(ram,0x998,0x11);require(!sample(-10));put(ram,0x998,0x0f);require(!sample(-10));put(ram,0x998,8);
  put(ram,a+14,0x100);require(!sample(-10));put(ram,a+14,0);
  put(ram,0x17a2,1);require(!sample(-10));put(ram,0x17a2,0);
  put(ram,a+38,100);require(sample(-10)->color==992);put(ram,a+38,0);
  put(rom,pos+5,0x4000);require(!sample(-10));put(rom,pos+5,0);
  // A multipart entry offsets an ordinary map by -16 pixels.
  put(ram,a+16,4);put(ram,a+22,0x9100);unsigned ext=pos+256;
  put(rom,ext,1);put(rom,ext+2,65520);put(rom,ext+6,0x9000);
  require(sample(-26) && sample(-26)->color==31);require(!sample(-10));
  put(rom,pos,0xfffe);require(!sample(-26));put(rom,pos,1);
  put(ram,a+16,0);put(ram,a+22,0x9000);put(ram,a+2,2);put(rom,pos+2,0x81f8);
  // Onscreen enemies need an actual matching native OAM witness.
  require(!sample(-6));ram[0x370]=250;ram[0x371]=60;put(ram,0x372,0x200);ram[0x570]=3;
  require(sample(-6) && sample(-6)->color==31);
  put(ram,a+2,260);put(rom,pos+2,0x8000);ram[0x370]=4;
  require(sample(260) && sample(260)->color==31);
  put(rom,pos,129);require(!sample(260));put(rom,pos,1);
  put(ram,a+22,0xffff);require(!sample(260));
  // Two map palettes must survive a matching native OAM witness.
  put(ram,a+22,0x9000);put(ram,a+2,250);put(ram,a+30,0);
  put(rom,pos,2);put(rom,pos+2,0);rom[pos+4]=0;put(rom,pos+5,0x200);
  put(rom,pos+7,8);rom[pos+9]=0;put(rom,pos+10,0x400);
  ram[0x370]=250;ram[0x371]=60;put(ram,0x372,0x200);ram[0x570]=0;
  put(pal,(128+32+1)*2,992);require(sample(258) && sample(258)->color==992);
  put(ram,a+22,0xffff);
  unsigned shot=0x13*32768+0x1000;
  put(rom,shot,1);put(rom,shot+2,0x8000);put(rom,shot+5,0x200);
  put(ram,0xc40,1);put(ram,0xcb8,0x9000);put(ram,0xb64,260);put(ram,0xb78,60);
  put(ram,0x5b6,1);require(sample(260) && sample(260)->color==31);
  put(ram,0x5b6,0);require(!sample(260));
  put(ram,0xc18,0x100);require(sample(260) && sample(260)->color==31);
  put(ram,0xb64,320);require(!sample(320));
  put(ram,0xc40,0);require(!sample(260));
  unsigned drop=0x0d*32768+0x1000;
  put(rom,drop,1);put(rom,drop+2,0x8000);put(rom,drop+5,0);
  put(ram,0x1997,0xf337);put(ram,0x1a03,0xefe0);put(ram,0x1b6b,0x9000);
  put(ram,0x19bb,0x200);put(ram,0x1a4b,260);put(ram,0x1a93,60);
  require(sample(260) && sample(260)->color==31);
  put(ram,0x1a4b,65526);require(sample(-10) && sample(-10)->color==31);
  put(ram,0x1a03,0xefdf);require(!sample(-10));put(ram,0x1a03,0xefe0);
  put(ram,0x1997,0);require(!sample(-10));put(ram,0x1997,0xf337);
  put(rom,drop,0);require(!sample(-10));put(rom,drop,1);
  // Draygon's wall-turret charge and fired shot share one live projectile ID.
  // Scope it to the arena, keep unrelated projectile IDs and native center out.
  put(ram,0x1a03,0x8d54);put(ram,0x1997,0x8e5e);put(ram,0x79b,0xda60);
  require(sample(-10) && sample(-10)->color==31);
  put(ram,0x1a4b,280);require(sample(280) && sample(280)->color==31);
  put(ram,0x1a4b,100);require(!sample(100));
  put(ram,0x1a4b,65526);put(ram,0x79b,0xd78f);require(!sample(-10));
  put(ram,0x79b,0xda60);put(ram,0x1997,0x8e50);require(sample(-10) && sample(-10)->color==31);
  put(ram,0x1a4b,280);require(sample(280) && sample(280)->color==31);
  put(ram,0x1a4b,65526);put(ram,0x79b,0xd78f);require(!sample(-10));
  put(ram,0x79b,0xda60);put(ram,0x1997,0x8e4f);require(!sample(-10));
  put(ram,0x1997,0);require(!sample(-10));
  put(ram,0x1997,0xf337);put(ram,0x1a03,0xefe0);
  put(rom,0x06*32768+0x46b+18*4,1);put(ram,0x183e,18);put(ram,0x1840,2);
  require(sample(-11) && sample(-11)->color==31);require(!sample(-3));
  std::cout<<"PASS: ordinary/extended maps, palette/freeze/flip, native witness, suppression, native-center exclusion and invalid pointers\n";
}
