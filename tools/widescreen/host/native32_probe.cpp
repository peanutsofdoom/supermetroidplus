// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok){if(!ok)throw std::runtime_error("native32 equivalence");}
int main(int argc,char** argv) {
  require(argc==2);
  std::vector<uint8_t> regs(205),ram(131072),vram(65536),palette(512),rgb(512*480*3);
  std::ifstream f(argv[1],std::ios::binary);f.read((char*)regs.data(),regs.size());require(bool(f));
  ram[0x998]=8;ram[0x7a5]=16;ram[0x7a7]=16;
  for(unsigned blur=0;blur<2;blur++)for(unsigned stride:{512u,520u}) {
    std::vector<uint32_t> native(stride*480,0xabcdef);
    auto color=[](unsigned x,unsigned y){return (x*7919+y*131)&0xffffff;};
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<256;x++) {
      uint32_t a=color(x,y/2),b=color(std::min(x+1,255u),y/2),odd=a;
      if(blur){odd=0;for(unsigned shift:{0u,8u,16u})odd|=((((a>>shift)&255)+((b>>shift)&255))/2)<<shift;}
      native[y*stride+x*2]=a|((y&1?0x55u:0xaau)<<24);
      native[y*stride+x*2+1]=odd|((y&1?0xffu:0u)<<24);
    }
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++)for(unsigned c=0;c<3;c++)
      rgb[(y*512+x)*3+c]=native[y*stride+x]>>(16-c*8);
    void* ctx=smpwide_create();require(ctx);
    require(smpwide_begin(ctx)&&smpwide_owner(ctx,{1,ram.data(),ram.size()})&&smpwide_upload(ctx,1));
    for(unsigned y=0;y<=224;y++)require(smpwide_raster(ctx,y,1,{1,vram.data(),vram.size()},{1,palette.data(),palette.size()},{1,regs.data(),regs.size()}));
    require(smpwide_compose32(ctx,native.data(),stride*4,512,480,blur));
    auto out=smpwide_output(ctx);std::vector<uint8_t> actual(out.data,out.data+out.size);
    require(smpwide_recompose(ctx,rgb.data(),rgb.size(),blur));out=smpwide_output(ctx);
    require(std::equal(actual.begin(),actual.end(),out.data));
    smpwide_destroy(ctx);
  }
  std::cout<<"PASS native32 vs RGB composition: blur, alpha, padded pitch and parallel rows\n";
}
