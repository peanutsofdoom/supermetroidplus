#include "mode1.hpp"
#include "../host/native_edge_patch.hpp"
#include <cassert>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
struct Background {Pixel pixel(int,int) const{return {992,1,true,true};}};
int main(){
  std::vector<uint8_t> ram(131072),rom(3145728),v(65536),pal(512),r(205);
  auto reg=[&](Reg k,unsigned value){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=value>>(i*8);};
  put(ram,0x998,8);put(ram,0x79b,0xb62b);put(ram,0x7bb,0xb638);
  for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
  unsigned a=0xf78,map=0x32*32768+0x1000;
  put(ram,a,0xf593);put(ram,a+2,256);put(ram,a+6,60);put(ram,a+10,16);
  put(ram,a+22,0x9000);put(ram,a+30,0x200);put(ram,a+34,5);put(ram,a+46,0xb2);
  put(rom,map,1);put(rom,map+2,0x81fc);put(rom,map+5,0x2000);
  for(unsigned t:{0u,1u,4u,5u,16u,17u,20u,21u})for(unsigned y=0;y<8;y++)v[t*32+y*2]=255;
  put(pal,(128+16+1)*2,31);
  reg(Reg::bgMode,1);reg(Reg::displayBrightness,15);reg(Reg::obj_aboveEnable,1);reg(Reg::obj_priority2,6);
  EnemySampler right(ram,rom,v,pal,r);
  assert(right.hasCenterFill() && right.pixel(254,61,true) && !right.pixel(254,61) && right.pixel(256,61));
  assert(!right.nativeObjectAt(254,61));
  reg(Reg::aboveEnable,1);reg(Reg::priority1,8);Background bg;
  assert((Mode1<Background>(bg,v,pal,r,61,0,0,{},nullptr,{},&right).pixel(254,true).rgb==std::array<uint8_t,3>{0,248,0}));
  reg(Reg::priority1,4);
  assert((Mode1<Background>(bg,v,pal,r,61,0,0,{},nullptr,{},&right).pixel(254,true).rgb==std::array<uint8_t,3>{248,0,0}));
  ram[0x370]=254;ram[0x371]=60;put(ram,0x372,0x2204);ram[0x570]=2;
  assert(EnemySampler(ram,rom,v,pal,r).nativeObjectAt(254,61));
  ram[0x371]=240;put(ram,a+2,uint16_t(-2));put(rom,map+2,0x8000);
  assert(EnemySampler(ram,rom,v,pal,r).pixel(0,61,true));
  put(ram,a+2,0);assert(!EnemySampler(ram,rom,v,pal,r).hasCenterFill());
  put(ram,a+2,uint16_t(-2));put(ram,0x7bb,0xb639);assert(!EnemySampler(ram,rom,v,pal,r).hasCenterFill());put(ram,0x7bb,0xb638);
  put(ram,a,0xe000);assert(!EnemySampler(ram,rom,v,pal,r).hasCenterFill());
  put(ram,a,0xf593);put(ram,a+14,0x100);assert(!EnemySampler(ram,rom,v,pal,r).hasCenterFill());
  std::vector<uint8_t> rgb(342*224*3),mask(256*224);std::vector<uint32_t> native(512,0x102030);
  for(unsigned x=0;x<256;x++){auto p=(x+43)*3;rgb[p]=16;rgb[p+1]=32;rgb[p+2]=48;}
  mask[254]=1;rgb[(254+43)*3]=240;
  SMPWideOutput wide{rgb.data(),rgb.size(),342,224,0,mask.data(),mask.size(),1};
  assert(smpNativeEdgeSample(native.data(),506,0,wide)==0x102030);
  assert(smpNativeEdgeSample(native.data(),507,0,wide)==0x802030);
  assert(smpNativeEdgeSample(native.data(),508,0,wide)==0xf02030);
  assert(smpNativeEdgeSample(native.data(),509,0,wide)==0x802030);
  assert(smpNativeEdgeSample(native.data(),510,0,wide)==0x102030);
  std::cout<<"PASS both origin crossings, gold-ninja-only scope, suppression, foreground priority, native-object protection and exact blur footprint\n";
}
