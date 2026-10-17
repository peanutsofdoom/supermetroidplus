// SPDX-License-Identifier: GPL-3.0-or-later
// Private fixture runner, not an emulator frontend or interactive-delivery gate.
#include "terrain.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

static std::vector<uint8_t> read(std::istream& in,size_t count) {
  std::vector<uint8_t> data(count);in.read(reinterpret_cast<char*>(data.data()),count);
  if(!in) throw std::runtime_error("truncated fixture");
  return data;
}
static uint32_t number(std::istream& in) {
  auto b=read(in,4);return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;
}
int main(int argc,char** argv) {
  try {
    if(argc!=3) throw std::runtime_error("usage: terrain-probe fixture output");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
    if(!in || !out) throw std::runtime_error("fixture/output open failed");
    if(number(in)!=0x31544d53) throw std::runtime_error("unknown fixture version");
    auto cases=number(in);uint64_t checked=0;double milliseconds=0;
    if(!cases || cases>10000) throw std::runtime_error("invalid case count");
    for(unsigned c=0;c<cases;c++) {
      auto ram=read(in,131072),vram=read(in,65536),palette=read(in,512);
      const auto originalRam=ram,originalVram=vram,originalPalette=palette;
      auto base=number(in),count=number(in);
      if(count>1000000) throw std::runtime_error("unbounded coordinate count");
      std::vector<std::array<int32_t,2>> positions(count);
      for(auto& position:positions) {position[0]=int32_t(number(in));position[1]=int32_t(number(in));}
      SMPWide::Terrain terrain(ram,vram,palette,base);
      std::vector<std::array<uint8_t,7>> pixels(count);
      auto start=std::chrono::steady_clock::now();
      for(unsigned n=0;n<count;n++) {
        auto x=positions[n][0],y=positions[n][1];uint16_t entry=0;
        bool valid=terrain.entry(x,y,entry);auto pixel=terrain.pixel(x,y);
        pixels[n]={uint8_t(valid),uint8_t(entry),uint8_t(entry>>8),uint8_t(pixel.color),
                   uint8_t(pixel.color>>8),pixel.priority,uint8_t(pixel.opaque)};
      }
      milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      if(ram!=originalRam || vram!=originalVram || palette!=originalPalette)
        throw std::runtime_error("kernel mutated an input image");
      for(auto pixel:pixels) out.write(reinterpret_cast<const char*>(pixel.data()),pixel.size());
      checked+=count;
    }
    if(in.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("trailing fixture data");
    out.close();if(!out) throw std::runtime_error("output write failed");
    std::cout<<"{\"cases\":"<<cases<<",\"positions\":"<<checked<<",\"kernel_ms\":"<<milliseconds<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
