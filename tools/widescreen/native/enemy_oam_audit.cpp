#define EnemySampler AuditEnemySampler
#include "enemy_objects.hpp"
#undef EnemySampler
using namespace SMPWide;
extern "C" unsigned audit_oam(const uint8_t* ram,const uint8_t* rom,const uint8_t* v,const uint8_t* c,const uint8_t* r,int* info){
 Bytes rb(ram,131072),vb(v,65536),cb(c,512),regs(r,205);Registers reg(regs);AuditEnemySampler sampler(rb,Bytes(rom,3145728),vb,cb,regs);unsigned bad=0;
 for(int y=32;y<224;y++)for(int x=-43;x<299;x++){if(x>=0 && x<256)continue;
  for(unsigned i=0;i<128;i++){
   unsigned hi=ram[0x570+i/4]>>(i%4*2),size=hi&2?16:8,px=ram[0x370+i*4]|((hi&1)<<8),py=ram[0x371+i*4],a=word(rb,0x372+i*4);
   unsigned dx=(unsigned(x)-px)&511,dy=(unsigned(y)-py)&255;if(dx>=size || dy>=size)continue;
   if(a&0x4000)dx=size-1-dx;if(a&0x8000)dy=size-1-dy;
   unsigned base=reg.get(Reg::obj_tiledataAddress);if(a&256)base+=(1+reg.get(Reg::obj_nameselect))<<12;
   unsigned tile=(((a&255)/16+dy/8)&15)*16+((a+dx/8)&15),addr=((base+tile*16)&32767)*2,index=0;
   for(unsigned bit=0;bit<4;bit++)index|=((v[(addr+bit/2*16+dy%8*2+bit%2)&65535]>>(7-dx%8))&1)<<bit;
   if(!index)continue;
   unsigned color=word(cb,(128+((a>>9)&7)*16+index)*2)&32767;auto got=sampler.pixel(x,y+1);
   if(!got || got->color!=color){if(!bad){info[0]=x;info[1]=y;info[2]=color;info[3]=got?got->color:-1;info[4]=a;info[5]=i;}bad++;}break;
  }
 }
 return bad;
}
