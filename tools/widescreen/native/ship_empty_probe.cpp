// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_objects.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv) {
 try {
  if(argc!=2)throw std::runtime_error("owner directory");
  unsigned owners=0,negatives=0;
  std::vector<uint8_t> maps(0x272),oam(544),vram(65536),palette(512),regs(205);
  for(auto& file:std::filesystem::directory_iterator(argv[1])) {
   if(file.path().extension()!=".wram")continue;
   std::ifstream f(file.path(),std::ios::binary);
   std::vector<uint8_t> ram{std::istreambuf_iterator<char>(f),{}};
   for(unsigned a:{0xf78,0xfb8,0xff8})
    if(SMPWide::word(ram,a+22)!=0x804d)throw std::runtime_error("fixture not empty");
   SMPWide::ShipSampler ship(ram,maps,oam,vram,palette,regs);
   if(!ship.descriptors().empty())throw std::runtime_error("empty map drew descriptors");
   for(unsigned y=1;y<=224;y++)for(int x=-43;x<299;x++)
    if(ship.pixel(x,y))throw std::runtime_error("empty map drew pixel");
   if(!owners)for(unsigned a:{0xf78,0xfb8,0xff8}) {
    for(auto mutation:std::vector<std::pair<unsigned,unsigned>>{
         {22,0},{22,0x804c},{22,0x804e},{22,0xad80},{22,0xaff2},
         {0,0},{46,0xa3},{16,4}}) {
     auto bad=ram;unsigned p=a+mutation.first,value=mutation.second;
     bad[p]=value;bad[p+1]=value>>8;
     bool rejected=false;
     try {SMPWide::ShipSampler invalid(bad,maps,oam,vram,palette,regs);}
     catch(const std::invalid_argument&){rejected=true;}
     if(!rejected)throw std::runtime_error("negative accepted");
     negatives++;
    }
   }
   owners++;
  }
  if(owners!=229 || negatives!=24)throw std::runtime_error("fixture count");
  std::cout<<"{\"passed\":true,\"owners\":"<<owners<<",\"empty_pixel_positions\":"<<uint64_t(owners)*224*342<<",\"negative_cases\":"<<negatives<<"}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
