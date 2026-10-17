// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_objects.hpp"
#include "mode1.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv) {
  try {
    if(argc!=8 && argc!=9)throw std::runtime_error("ram maps oam vram palette registers output [previous]");
    auto ram=read(argv[1]),maps=read(argv[2]),oam=read(argv[3]),vram=read(argv[4]),palette=read(argv[5]),reg=read(argv[6]);
    auto previous=argc==9?read(argv[8]):std::vector<uint8_t>{};
    SMPWide::ShipSampler ship(ram,maps,oam,vram,palette,reg,previous);
    std::ofstream out(argv[7],std::ios::binary);
    for(unsigned line=1;line<=224;line++)for(int x=-43;x<299;x++) {
      auto p=ship.pixel(x,line);
      uint8_t b[5]={uint8_t(bool(p)),uint8_t(p?p->color:0),uint8_t(p?p->color>>8:0),uint8_t(p?p->priority:0),uint8_t(p?p->palette:0)};
      out.write(reinterpret_cast<char*>(b),5);
    }
    if(!out)throw std::runtime_error("output");
    std::ofstream composed(std::string(argv[7])+".mode1",std::ios::binary);
    SMPWide::Registers regs(reg);
    SMPWide::Terrain terrain(ram,vram,palette,regs.get(SMPWide::Reg::tiledataAddress)*2);
    for(unsigned line=32;line<=224;line++) {
      SMPWide::Mode1 row(terrain,vram,palette,reg,line,SMPWide::word(ram,0x911),SMPWide::word(ram,0x915),{},&ship);
      for(int x=-43;x<299;x++) {
        auto p=row.pixel(x);uint8_t b[4]={uint8_t(p.valid),p.rgb[0],p.rgb[1],p.rgb[2]};
        composed.write(reinterpret_cast<char*>(b),4);
      }
    }
    if(!composed)throw std::runtime_error("composition output");
    std::cout<<"PASS "<<ship.descriptors().size()<<" ship descriptors\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
