#include "frame_pipeline.hpp"
#include <iostream>
#include <fstream>
using namespace SMPWide;
void put(std::vector<uint8_t>&b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>&b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(int argc,char** argv){
 std::vector<uint8_t> rom;if(argc==2){std::ifstream f(argv[1],std::ios::binary);rom={std::istreambuf_iterator<char>(f),{}};}
 std::vector<uint8_t> ram(131072),v(65536),pal(512),regs(205),native(512*480*3);
 for(auto q:{std::pair{0x79b,0xa923},std::pair{0x7bb,0xa930},std::pair{0x998,8},std::pair{0x7a5,208},std::pair{0x7a7,48},std::pair{0x911,3072},std::pair{0x183e,0x14},std::pair{0x1840,20}})put(ram,q.first,q.second);
 for(unsigned q=0;q<4;q++)put(ram,0xa000+q*2,1);
 for(unsigned i=0;i<2048;i++)put(v,0x8000+i*2,2);
 for(unsigned y=0;y<8;y++){v[32+y*2]=255;v[64+y*2+1]=255;}put(pal,2,31);put(pal,4,992);
 reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);reg(regs,Reg::screenAddress,0x4000);reg(regs,Reg::screenSize,1);reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,6);reg(regs,Reg::hoffset,3071);
 auto run=[&](bool matching){FramePipeline p;if(!rom.empty())p.configureEnemies(rom);p.begin();p.owner(1,ram);p.upload(1);for(unsigned y=0;y<=224;y++)p.raster(y,1,memoryImage(v,65536),memoryImage(pal,512),memoryImage(regs,205));p.seal();std::fill(native.begin(),native.end(),0);for(unsigned y=31;y<224;y++)for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)native[(((y+8)*2+dy)*512+dx)*3+1]=matching?248:16;return p.compose(native,false);};
 auto good=run(true);unsigned count=0;for(auto b:good.centerMask)count+=b;
 if(count!=193)throw std::runtime_error("Expected exactly one repaired column");
 for(unsigned y=31;y<224;y++)if(good.rgb[(y*342+43)*3]!=248 || good.rgb[(y*342+43)*3+1])throw std::runtime_error("Full-room color missing");
 if(!run(false).centerMask.empty())throw std::runtime_error("Mismatching native pixel overwritten");
 put(ram,0x1840,0);if(!run(true).centerMask.empty())throw std::runtime_error("Inactive quake repaired");
 put(ram,0x1840,20);put(ram,0x79b,0xa922);if(!run(true).centerMask.empty())throw std::runtime_error("Other room repaired");
 if(!rom.empty()){
  put(ram,0x79b,0xa923);ram[0x370]=0;ram[0x371]=31;put(ram,0x372,2);
  auto sprite=run(true);unsigned pixels=0;for(auto b:sprite.centerMask)pixels+=b;
  if(pixels!=185)throw std::runtime_error("Native sprite overlap not preserved");
 }
 std::cout<<"PASS quake single-column fill, native pixel witness, inactive/other-room guards\n";
}
