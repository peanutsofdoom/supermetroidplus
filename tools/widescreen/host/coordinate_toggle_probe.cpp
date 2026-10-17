// SPDX-License-Identifier: GPL-3.0-or-later
#include "coordinate_overlay.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{
  SmpCoordinateUnderlay backup;backup.restore(nullptr,0,0);
  for(auto shape:{std::array<unsigned,3>{684,224,0},{2048,448,0},{512,480,16},{256,240,8}}) {
    auto [w,h,crop]=shape;std::vector<uint32_t> frame(size_t(w)*h);
    for(size_t i=0;i<frame.size();i++)frame[i]=uint32_t(i*97+31);
    auto clean=frame;
    for(unsigned n=0;n<20;n++) {
      backup.capture(frame.data(),w,h,crop);
      smpDrawCoordinatePanel(frame.data(),w,h,crop,"D245 R:91F8 S:922D G:08 W F:000001","P:01152,01088 C:01024,01024 FPS:60.0");
      if(frame==clean)throw std::runtime_error("HUD absent");
      backup.restore(frame.data(),w,h);
      if(frame!=clean)throw std::runtime_error("HUD pixels left behind");
    }
    backup.capture(frame.data(),w,h,crop);std::fill(frame.begin(),frame.end(),123);backup.reset();backup.restore(frame.data(),w,h);
    if(std::any_of(frame.begin(),frame.end(),[](auto x){return x!=123;}))throw std::runtime_error("Old underlay overwrote new frame");
    backup.capture(frame.data(),w,h,crop);auto other=clean;backup.restore(other.data(),w,h);
    if(other!=clean)throw std::runtime_error("Foreign buffer changed");
  }
  std::cout<<"PASS 80 HUD restore cycles, compact/full/native sizes, new-frame and owner guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
