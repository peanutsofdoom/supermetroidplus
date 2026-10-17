// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <fstream>
#include <iostream>
static std::vector<uint8_t> read(std::istream& in,size_t count) {
  std::vector<uint8_t> data(count);in.read(reinterpret_cast<char*>(data.data()),count);
  if(!in)throw std::runtime_error("truncated fixture");
  return data;
}
static uint64_t number(std::istream& in,unsigned size=4) {
  auto b=read(in,size);uint64_t n=0;
  for(unsigned i=0;i<size;i++)n|=uint64_t(b[i])<<(i*8);
  return n;
}
int main(int argc,char** argv) {
  try {
    if(argc!=3)throw std::runtime_error("usage: timeline-probe fixture output");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
    if(!in || !out)throw std::runtime_error("fixture/output open failed");
    if(number(in)!=0x31564d53)throw std::runtime_error("unknown fixture version");
    auto count=number(in);if(!count || count>10000)throw std::runtime_error("invalid row count");
    uint64_t transactions=0,writes=0;
    for(unsigned c=0;c<count;c++) {
      auto joined=number(in);if(joined>1)throw std::runtime_error("invalid join flag");
      int exitX=int(number(in)),exitY=int(number(in));
      auto assets=read(in,131072),view=read(in,131072),prior=read(in,joined?131072:0);
      auto vram=read(in,65536),palette=read(in,512),registers=read(in,205);
      const auto beforeVram=vram,beforePalette=palette,beforeRegisters=registers;
      unsigned line=unsigned(number(in)),flags=unsigned(number(in)),events=unsigned(number(in));
      if(flags>7 || events>100000)throw std::runtime_error("invalid row metadata");
      SMPWide::SceneSelection scene{std::make_shared<const SMPWide::OwnerImage>(1,assets),
        std::make_shared<const SMPWide::OwnerImage>(2,view),{},SMPWide::SceneKind::Ordinary,exitX,exitY};
      if(joined)scene.previous=std::make_shared<const SMPWide::OwnerImage>(3,prior);
      auto v=SMPWide::memoryImage(vram,65536),p=SMPWide::memoryImage(palette,512);
      SMPWide::RowTimeline row(scene,v,p,registers,line);
      if(flags&1)row.pixel(0);
      for(unsigned e=0;e<events;e++) {
        auto cycle=number(in,8);unsigned h=unsigned(number(in)),address=unsigned(number(in));
        auto after=read(in,205);unsigned memoryWrites=unsigned(number(in));
        if(memoryWrites>1000)throw std::runtime_error("invalid transaction size");
        for(unsigned w=0;w<memoryWrites;w++) {
          auto clock=number(in,8);unsigned space=unsigned(number(in)),a=unsigned(number(in)),width=unsigned(number(in)),value=unsigned(number(in));
          row.memoryWrite({clock,space,a,width,value});writes++;
        }
        row.registerWrite(cycle,h,address,after);transactions++;
      }
      if(flags&2)row.memoryWrite({1,1,0,1,0});
      row.finish();
      if(flags&4)row.memoryWrite({1,1,0,1,0});
      for(int x=-43;x<299;x++) {
        auto pixel=row.pixel(x);uint8_t value[]={uint8_t(pixel.valid),pixel.rgb[0],pixel.rgb[1],pixel.rgb[2]};
        out.write(reinterpret_cast<char*>(value),sizeof(value));
      }
      if(vram!=beforeVram || palette!=beforePalette || registers!=beforeRegisters || *v!=vram || *p!=palette ||
         scene.assets->bytes!=assets || scene.view->bytes!=view || (joined && scene.previous->bytes!=prior))
        throw std::runtime_error("timeline mutated an input image");
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing fixture data");
    out.close();if(!out)throw std::runtime_error("output write failed");
    std::cout<<"{\"rows\":"<<count<<",\"pixels\":"<<count*342<<",\"transactions\":"<<transactions<<",\"writes\":"<<writes<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
