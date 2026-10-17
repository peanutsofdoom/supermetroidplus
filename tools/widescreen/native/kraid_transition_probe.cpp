#include "frame_pipeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){
  std::vector<uint8_t> old(131072),next,oldV(65536),newV(65536),oldP(512),newP(512),regs(205),native(512*480*3);
  for(auto pair:{std::pair{0x998,8},std::pair{0x79b,0xa56b},std::pair{0x7bb,0xa578},std::pair{0x7a5,64},std::pair{0x7a7,32},std::pair{0x78d,0x91b6},std::pair{0x911,256},std::pair{0x915,256}})put(old,pair.first,pair.second);
  for(unsigned q=0;q<4;q++)put(old,0xa008+q*2,0x401);
  for(unsigned i=0;i<64*32;i++)put(old,0x19602+i*2,1);
  for(unsigned y=0;y<8;y++){oldV[y*2]=255;oldV[0x2000+32+y*2]=255;newV[y*2+1]=255;}
  put(oldP,2,31);put(oldP,34,1023);put(newP,4,31744);
  reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,4);
  reg(regs,Reg::bg2_tiledataAddress,0x1000);reg(regs,Reg::hoffset,256);reg(regs,Reg::voffset,256);
  auto vOld=memoryImage(oldV,65536),vNew=memoryImage(newV,65536),pOld=memoryImage(oldP,512),pNew=memoryImage(newP,512);
  FramePipeline pipeline;unsigned tag=0;
  auto frame=[&](const auto& ram,MemoryRef v,MemoryRef p){pipeline.begin();pipeline.owner(++tag,ram);pipeline.upload(tag);for(unsigned y=0;y<=224;y++)pipeline.raster(y,tag,v,p,memoryImage(regs,205));pipeline.seal();};
  frame(old,vOld,pOld);put(old,0x998,9);frame(old,vNew,pNew);
  next=old;put(next,0x998,11);put(next,0x99c,0xe38e);put(next,0x79b,0xa59f);put(next,0x7bb,0xa5b1);put(next,0x7a5,32);put(next,0x929,256);
  frame(next,vNew,pNew);put(next,0x99c,0xe3c0);put(next,0x911,65520);reg(regs,Reg::hoffset,65520);
  frame(next,vNew,pNew);auto result=pipeline.compose(native,false);
  for(unsigned y=32;y<224;y++) {
    auto left=(y*342)*3,right=(y*342+341)*3;
    if(result.rgb[left]!=248 || result.rgb[left+1] || result.rgb[left+2] || result.rgb[right] || result.rgb[right+1] || result.rgb[right+2]!=248)throw std::runtime_error("departing/incoming art mixed");
    for(unsigned x=43;x<299;x++)for(unsigned c=0;c<3;c++)if(result.rgb[(y*342+x)*3+c])throw std::runtime_error("native center changed");
  }
  SceneGeometry geometry(next,next,old,newV,newP,0,true,256,256,oldV,oldP,0,0x2000);
  auto bg=geometry.background(-43,100,0,0,0,65520,256);if(!bg || bg->color!=1023)throw std::runtime_error("departing custom BG2 lost");
  put(next,0x911,0);reg(regs,Reg::hoffset,0);frame(next,vNew,pNew);auto finished=pipeline.compose(native,false);
  if(finished.rgb[(100*342)*3] || finished.rgb[(100*342)*3+1] || finished.rgb[(100*342)*3+2])throw std::runtime_error("departing room lingered after final camera position");
  // Reverse Kraid -> corridor and both Varia links retain art and clip it once
  // the native viewport is fully inside the destination, in either direction.
  for(unsigned destination:{0xa56bu,0xa6e2u})for(unsigned dir:{0u,1u}) {
    auto prior=old,view=next;put(prior,0x79b,0xa59f);put(prior,0x7bb,0xa5cb);
    put(view,0x79b,destination);put(view,0x7bb,destination==0xa56b?0xa578:0xa6ef);put(view,0x791,dir);put(view,0x927,dir?256:0);put(view,0x911,dir?272:65520);
    SceneGeometry joined(view,view,prior,newV,newP,0,true,0,256,oldV,oldP,0,0x2000,regs);
    if(!joined.pixel(dir?528:-16,300).valid)throw std::runtime_error("reverse outgoing geometry lost early");
    put(view,0x911,dir?256:0);
    SceneGeometry end(view,view,prior,newV,newP,0,true,0,256,oldV,oldP,0,0x2000,regs);
    if(end.pixel(dir?512:-1,300).valid)throw std::runtime_error("reverse outgoing geometry lingered");
  }
  bool rejected=false;try{SceneGeometry missing(next,next,old,newV,newP,0,true,256,256);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("missing graphics accepted");
  pipeline.reset();rejected=false;try{pipeline.begin();pipeline.owner(100,next);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("reset retained stale history");
  std::cout<<"PASS: retained departing art/palette through staged tileset replacement, custom BG2, native center, missing history and reset\n";
}
