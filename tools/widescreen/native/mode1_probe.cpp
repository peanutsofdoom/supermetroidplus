// SPDX-License-Identifier: GPL-3.0-or-later
// Whole-row composition fixture runner; no emulation or guest mutation.
#include "mode1.hpp"
#include <chrono>
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
    if(argc!=3)throw std::runtime_error("usage: mode1-probe fixture output");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
    if(!in || !out)throw std::runtime_error("fixture/output open failed");
    if(number(in)!=0x314d4d53)throw std::runtime_error("unknown fixture version");
    auto cases=number(in);if(!cases || cases>10000)throw std::runtime_error("invalid case count");
    uint64_t pixels=0;double ms=0;
    for(unsigned c=0;c<cases;c++) {
      auto ram=read(in,131072),vram=read(in,65536),palette=read(in,512),registers=read(in,205);
      const auto oldRam=ram,oldVram=vram,oldPalette=palette,oldRegisters=registers;
      auto line=number(in),cx=number(in),cy=number(in);
      SMPWide::Registers r(registers);SMPWide::Terrain terrain(ram,vram,palette,r.get(SMPWide::Reg::tiledataAddress)*2);
      SMPWide::Mode1 row(terrain,vram,palette,registers,line,int(cx),int(cy));
      std::array<std::array<uint8_t,4>,342> values;
      auto start=std::chrono::steady_clock::now();
      for(int x=-43;x<299;x++) {
        auto pixel=row.pixel(x);values[x+43]={uint8_t(pixel.valid),pixel.rgb[0],pixel.rgb[1],pixel.rgb[2]};
      }
      ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      if(ram!=oldRam || vram!=oldVram || palette!=oldPalette || registers!=oldRegisters)
        throw std::runtime_error("composition mutated snapshots");
      for(auto value:values)out.write(reinterpret_cast<const char*>(value.data()),value.size());
      pixels+=values.size();
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing fixture data");
    out.close();if(!out)throw std::runtime_error("output write failed");
    std::cout<<"{\"rows\":"<<cases<<",\"pixels\":"<<pixels<<",\"composition_ms\":"<<ms<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
