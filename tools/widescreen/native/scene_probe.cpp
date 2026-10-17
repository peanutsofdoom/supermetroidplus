// SPDX-License-Identifier: GPL-3.0-or-later
#include "mode1.hpp"
#include "scene_geometry.hpp"
#include <fstream>
#include <iostream>
#include <vector>
static std::vector<uint8_t> read(std::istream& in,size_t count) {
  std::vector<uint8_t> data(count);in.read(reinterpret_cast<char*>(data.data()),count);
  if(!in)throw std::runtime_error("truncated fixture");
  return data;
}
static uint32_t number(std::istream& in) {
  auto b=read(in,4);return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;
}
int main(int argc,char** argv) {
  try {
    if(argc!=3)throw std::runtime_error("usage: scene-probe fixture output");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
    if(!in || !out)throw std::runtime_error("fixture/output open failed");
    if(number(in)!=0x31534d53)throw std::runtime_error("unknown fixture version");
    auto count=number(in);if(!count || count>10000)throw std::runtime_error("invalid row count");
    for(unsigned c=0;c<count;c++) {
      auto joined=number(in);if(joined>1)throw std::runtime_error("invalid join flag");
      int exitX=int(number(in)),exitY=int(number(in));
      auto assets=read(in,131072),view=read(in,131072),prior=read(in,joined?131072:0);
      auto vram=read(in,65536),palette=read(in,512),registers=read(in,205);
      const auto oldAssets=assets,oldView=view,oldPrior=prior,oldVram=vram,oldPalette=palette,oldRegisters=registers;
      auto line=number(in),cx=number(in),cy=number(in);
      SMPWide::Registers r(registers);
      SMPWide::SceneGeometry geometry(assets,view,prior,vram,palette,r.get(SMPWide::Reg::tiledataAddress)*2,joined,exitX,exitY);
      SMPWide::Mode1 row(geometry,vram,palette,registers,line,int(cx),int(cy));
      for(int x=-43;x<299;x++) {
        auto pixel=row.pixel(x);uint16_t tile=0;
        bool valid=geometry.entry(int(cx)+x,int(cy)+int(line),tile);
        uint8_t value[]={uint8_t(pixel.valid),pixel.rgb[0],pixel.rgb[1],pixel.rgb[2],uint8_t(valid),uint8_t(tile),uint8_t(tile>>8)};
        out.write(reinterpret_cast<char*>(value),sizeof(value));
      }
      if(assets!=oldAssets || view!=oldView || prior!=oldPrior || vram!=oldVram || palette!=oldPalette || registers!=oldRegisters)
        throw std::runtime_error("scene composition mutated snapshots");
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing fixture data");
    out.close();if(!out)throw std::runtime_error("output write failed");
    std::cout<<"{\"rows\":"<<count<<",\"pixels\":"<<count*342<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
