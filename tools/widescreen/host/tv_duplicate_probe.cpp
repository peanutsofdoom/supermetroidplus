#include "tv_presentation.hpp"
#include <cassert>
#include <iostream>
int main(){
  std::vector<uint32_t> native(512*480),actual,expected(2048*448);
  std::vector<uint8_t> rgb(342*224*3),mask(256*224);
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)native[y*512+x]=(x*7919+y/2*31)&0xffffff;
  for(unsigned i=0;i<rgb.size();i++)rgb[i]=i*37;
  SMPWideOutput wide{rgb.data(),rgb.size(),342,224,0,nullptr,0,0};
  for(unsigned test=0;test<3;test++) {
    if(test==1)native[89*512+123]^=0xffffff;
    if(test==2){mask[20*256+254]=1;wide.center_mask=mask.data();wide.center_mask_size=mask.size();wide.blur=1;}
    smpGuiTvSurface(native.data(),512*4,512,480,wide,actual);
    for(unsigned y=0;y<448;y++) {
      auto* row=expected.data()+y*2048;auto* center=native.data()+(y+16)*512;
      for(unsigned x=0;x<512;x++)row[256+x*3]=row[257+x*3]=row[258+x*3]=smpNativeEdgeSample(center,x,y/2,wide);
      for(unsigned x=0;x<256;x++){
        auto color=[&](unsigned logical){auto* p=rgb.data()+((y/2)*342+logical)*3;return unsigned(p[0])<<16|unsigned(p[1])<<8|p[2];};
        row[x]=color((x+2)/6);row[1792+x]=color((1794+x)/6);
      }
    }
    assert(actual==expected);
  }
  std::cout<<"PASS duplicate-row fast path, differing-row fallback and masked blur pixel parity\n";
}
