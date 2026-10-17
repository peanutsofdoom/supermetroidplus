#include "compact_upload.hpp"
#include "tv_presentation.hpp"
#include "coordinate_overlay.hpp"
#include <cassert>
#include <iostream>
int main(){
  std::vector<uint32_t> native(512*480),canonical,packed,direct,expanded;
  std::vector<uint8_t> rgb(342*224*3),mask(256*224);
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)native[y*512+x]=(x*7919+y/2*131)&0xffffff;
  for(unsigned i=0;i<rgb.size();i++)rgb[i]=i*37;
  mask[110*256+254]=1;
  SMPWideOutput wide{rgb.data(),rgb.size(),342,224,0,mask.data(),mask.size(),1};
  for(unsigned overlay=0;overlay<2;overlay++) {
    smpGuiTvSurface(native.data(),2048,512,480,wide,canonical);
    if(overlay)smpDrawCoordinatePanel(canonical.data(),2048,448,0,"D66 R:91F8 S:9247 G:08 W F:123456","P:01152,01088 C:01024,01024 FPS:59.9");
    smpPackTvUpload(canonical.data(),8192,packed);
    smpGuiCompactTvSurface(native.data(),2048,512,480,wide,direct);
    if(overlay)smpDrawCoordinatePanel(direct.data(),684,224,0,"D66 R:91F8 S:9247 G:08 W F:123456","P:01152,01088 C:01024,01024 FPS:59.9");
    assert(direct==packed);
    smpUnpackTvUpload(direct.data(),684*4,expanded);
    assert(expanded==canonical);
    for(unsigned y=0;y<448;y++)for(unsigned x=0;x<2048;x++)assert(canonical[y*2048+x]==packed[(y/2)*684+(x+2)/3]);
    for(unsigned w:{640,768,896,1024,1280,1366,1920,2560,3840}) {
      unsigned h=w*9/16;
      for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        unsigned sx=(uint64_t(x)*2+1)*2048/(w*2),sy=(uint64_t(y)*2+1)*448/(h*2);
        assert(canonical[sy*2048+sx]==packed[(sy/2)*684+(sx+2)/3]);
      }
    }
  }
  std::cout<<"PASS exact compact reconstruction of every TV pixel, HUD, repair blur and nine output sizes\n";
}
