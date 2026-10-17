#include "mode1.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(i*8);}
struct Vine {Pixel pixel(int,int)const{return {31,1,true,true};}};
int run(int argc,char** argv){
 if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});
 if(rom.size()!=3145728)return 3;
 std::vector<uint8_t> ram(131072),v(65536),pal(512),regs(205);
 put(ram,0x998,8);put(ram,0xf1f8+62,64);
 for(unsigned t=0;t<2048;t++)for(unsigned y=0;y<8;y++)v[t*32+y*2]=255;
 for(unsigned p=0;p<8;p++)put(pal,(128+p*16+1)*2,992);
 unsigned frames=0,pixels=0;
 for(unsigned id:{3u,6u,9u,0x37u}) {
  unsigned inst=word(rom,0x34*32768+0x3da8+id*2);
  for(unsigned n=0;n<50;n++,inst+=4){
   unsigned address=0x34*32768+(inst&32767);if(word(rom,address)&0x8000)break;
   unsigned map=word(rom,address+2),count=word(rom,0x34*32768+(map&32767));
   for(int origin:{-24,280}) {
    put(ram,0xef78+62,inst);put(ram,0xf0f8+62,uint16_t(origin));EnemySampler sampler(ram,rom,v,pal,regs);
    if(sampler.count()!=count)throw std::runtime_error("sprite effect descriptor mismatch");
    for(int x=origin-20;x<origin+20;x++)if(x<0 || x>=256)for(unsigned y=32;y<96;y++)if(auto p=sampler.pixel(x,y)){
      if(p->color!=992 || p->order>=smpObjectGroup(0))throw std::runtime_error("effect palette/order mismatch");pixels++;
    }
    if(sampler.pixel(128,64))throw std::runtime_error("native center changed");
   }
   frames++;
  }
 }
 if(!pixels)throw std::runtime_error("No real effect pixels");
 put(ram,0xef78+62,0);if(EnemySampler(ram,rom,v,pal,regs).count())throw std::runtime_error("deleted effects rendered");
 // Foreground vine stays in front of priority2 sprites; priority3 stays in front.
 for(unsigned i=0;i<7;i++)rom[0x34*32768+0x1100+i]=0;
 put(ram,0xef78+62,0x9000);put(rom,0x34*32768+0x1002,0x9100);put(rom,0x34*32768+0x1100,1);put(rom,0x34*32768+0x1102,0);
 put(ram,0xf0f8+62,280);reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority1,8);reg(regs,Reg::obj_aboveEnable,1);reg(regs,Reg::obj_priority2,6);reg(regs,Reg::obj_priority3,9);
 Vine vine;
 for(unsigned priority:{2u,3u}){
  put(rom,0x34*32768+0x1105,priority<<12);EnemySampler effects(ram,rom,v,pal,regs);
  Mode1<Vine> mode(vine,v,pal,regs,65,0,0,{},nullptr,{},&effects);
  auto p=mode.pixel(280);if(!p.valid || p.rgb!=(priority==2?std::array<uint8_t,3>{248,0,0}:std::array<uint8_t,3>{0,248,0}))throw std::runtime_error("vine priority changed");
 }
 std::cout<<"PASS "<<frames<<" real effect animation descriptors, "<<pixels<<" margin pixels, native exclusion/deletion and vine priority\n";return 0;
}

int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
