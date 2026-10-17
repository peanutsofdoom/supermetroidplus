#include "coordinate_overlay.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
int main(){
  for(auto size:{std::array<unsigned,3>{2048,448,0},std::array<unsigned,3>{512,480,16},std::array<unsigned,3>{256,240,8}}) {
    unsigned w=size[0],h=size[1],crop=size[2];
    std::vector<uint32_t> input((w+8)*h,0x123456),output;
    auto original=input;
    auto p=smpCoordinateSurface(input.data(),(w+8)*4,w,h,crop,"D8 R:92FD S:932E G:08 W F:012345","P:01234,05678 C:01024,05120",output);
    if(input!=original || p!=output.data() || output.size()!=size_t(w)*h)throw std::runtime_error("overlay ownership");
    unsigned sy=h>=448?2:1,top=h-crop-19*sy,changed=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
      bool differs=output[y*w+x]!=0x123456;
      if(differs && (y<top || y>=h-crop))throw std::runtime_error("overlay escaped panel");
      changed+=differs;
    }
    if(!changed)throw std::runtime_error("empty overlay");
    if(w==2048) {
      std::ofstream image("coordinate-layout.ppm",std::ios::binary);image<<"P6\n896 504\n255\n";
      for(unsigned y=0;y<504;y++)for(unsigned x=0;x<896;x++) {
        auto rgb=output[(y*h/504)*w+x*w/896];
        for(int shift:{16,8,0})image.put(char(rgb>>shift));
      }
    }
  }
  std::cout<<"PASS: wide/native surfaces, padded pitch, crop, bounded panel and immutable source\n";
}
