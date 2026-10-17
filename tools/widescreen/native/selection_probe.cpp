// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_selection.hpp"
#include <fstream>
#include <iostream>
static std::vector<uint8_t> read(std::istream& in,size_t count) {
  std::vector<uint8_t> data(count);in.read(reinterpret_cast<char*>(data.data()),count);
  if(!in)throw std::runtime_error("truncated fixture");
  return data;
}
static uint32_t number(std::istream& in) {
  auto b=read(in,4);return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;
}
static void write(std::ostream& out,uint64_t value,unsigned size) {
  for(unsigned i=0;i<size;i++)out.put(char((value>>(8*i))&255));
}
int main(int argc,char** argv) {
  try {
    if(argc!=3)throw std::runtime_error("usage: selection-probe fixture output");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
    if(!in || !out)throw std::runtime_error("fixture/output open failed");
    if(number(in)!=0x31524d53)throw std::runtime_error("unknown fixture version");
    auto count=number(in);if(!count || count>100000)throw std::runtime_error("invalid event count");
    SMPWide::SceneSelector selector;unsigned selections=0,resets=0;
    for(unsigned i=0;i<count;i++) {
      auto command=number(in);
      if(command==0) {selector.reset();resets++;continue;}
      if(command!=1)throw std::runtime_error("unknown command");
      auto tag=number(in);auto data=read(in,131072);const auto before=data;
      auto owner=std::make_shared<const SMPWide::OwnerImage>(tag,data);
      auto scene=selector.select(owner);
      if(data!=before || owner->bytes!=before)throw std::runtime_error("selection mutated memory");
      write(out,scene.assets->tag,8);write(out,scene.view->tag,8);
      write(out,scene.previous?scene.previous->tag:UINT64_MAX,8);
      write(out,unsigned(scene.kind),4);write(out,uint32_t(scene.exitX),4);write(out,uint32_t(scene.exitY),4);
      selections++;
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing fixture data");
    out.close();if(!out)throw std::runtime_error("output write failed");
    std::cout<<"{\"selections\":"<<selections<<",\"resets\":"<<resets<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
