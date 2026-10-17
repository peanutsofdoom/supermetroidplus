// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include <iostream>
int main() {
  try {
    const unsigned stride=520;
    std::vector<uint32_t> native(stride*480);
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<stride;x++)native[y*stride+x]=0x7f000000u|(y<<12)|x;
    std::vector<uint8_t> wide(342*224*3);
    for(unsigned i=0;i<wide.size();i++)wide[i]=uint8_t(i*37);
    auto before=native;auto originalWide=wide;
    SMPWideOutput frame{wide.data(),wide.size(),342,224,0};
    auto out=smpGuiSurface(native.data(),stride*4,512,480,frame);
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)
      if(out[y*684+x+86]!=native[y*stride+x])throw std::runtime_error("center differs");
    for(unsigned y=0;y<224;y++)for(unsigned x=0;x<342;x++) {
      if(x>=43 && x<299)continue;
      auto p=&wide[(y*342+x)*3];uint32_t expected=uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];
      for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)
        if(out[((y+8)*2+dy)*684+x*2+dx]!=expected)throw std::runtime_error("margin differs");
    }
    for(unsigned y=0;y<480;y++)if(y<16 || y>=464)
      for(unsigned x=0;x<684;x++)if((x<86 || x>=598) && out[y*684+x])throw std::runtime_error("padding differs");
    if(native!=before || wide!=originalWide || smpGuiSurface(native.data(),stride*4,512,480,frame)!=out)
      throw std::runtime_error("mutation or nondeterminism");
    bool rejected=false;try{smpGuiSurface(native.data(),stride*4,256,480,frame);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("invalid geometry accepted");
    std::cout<<"PASS: 245760 exact native-center pixels, 77056 margin samples, padding, pitch, immutable/repeat and invalid geometry\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
