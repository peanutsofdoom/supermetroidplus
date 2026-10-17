// SPDX-License-Identifier: GPL-3.0-or-later
#include "tv_presentation.hpp"
#include "presentation.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
int main() {
  try {
    auto require=[](bool p){if(!p)throw std::runtime_error("TV presentation check failed");};
    constexpr unsigned stride=520;
    std::vector<uint32_t> native(stride*480);
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<stride;x++)native[y*stride+x]=0x7f000000u|(y<<12)|x;
    std::vector<uint8_t> rgb(342*224*3);
    for(unsigned i=0;i<rgb.size();i++)rgb[i]=uint8_t(i*37);
    const auto before=native;const auto originalRgb=rgb;
    SMPWideOutput wide{rgb.data(),rgb.size(),342,224,0};std::vector<uint32_t> out;
    smpGuiTvSurface(native.data(),stride*4,512,480,wide,out);const auto first=out;
    require(out.size()==2048*448);
    for(unsigned y=0;y<448;y++)for(unsigned x=0;x<512;x++)for(unsigned n=0;n<3;n++)
      require(out[y*2048+256+x*3+n]==native[(y+16)*stride+x]);
    std::set<unsigned> marginColumns;
    for(unsigned y=0;y<448;y++)for(unsigned x=0;x<2048;x++) {
      if(x>=256 && x<1792)continue;
      unsigned logical=(x+2)/6;marginColumns.insert(logical);
      const auto* p=&rgb[((y/2)*342+logical)*3];uint32_t expected=uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];
      require(out[y*2048+x]==expected);
    }
    require(marginColumns.size()==86 && *marginColumns.begin()==0 && *marginColumns.rbegin()==341);
    smpGuiTvSurface(native.data(),stride*4,512,480,wide,out);
    require(out==first && native==before && rgb==originalRgb);
    for(auto size:{SMPTvSize{1920,1080},SMPTvSize{3840,2160}}) {
      auto w=smpTvFit(size.width,size.height,true),n=smpTvFit(size.width,size.height,false);
      require(w.width==size.width && w.height==size.height);
      require(n.height==size.height && n.width==size.height*4/3);
      require(w.width*1536/2048==n.width);
      require(w.width*256/2048==(size.width-n.width)/2);
      // Match the driver's nearest sampling: every displayed center pixel is
      // identical when toggling between wide and native4:3 at the same height.
      for(unsigned y=0;y<n.height;y++)for(unsigned x=0;x<n.width;x++) {
        auto sy=unsigned(uint64_t(2*y+1)*448/(2*n.height));
        auto nx=unsigned(uint64_t(2*x+1)*512/(2*n.width));
        auto tx=unsigned(uint64_t(2*(x+(size.width-n.width)/2)+1)*2048/(2*w.width));
        require(out[sy*2048+tx]==native[(sy+16)*stride+nx]);
      }
    }
    auto ultrawide=smpTvFit(2560,1080,true);require(ultrawide.width==1920 && ultrawide.height==1080);
    auto small=smpTvFit(800,600,true);require(small.width==800 && small.height==450);
    require(smpTvFit(0,0,true).width==0);
    require(smpTvCrop(448,true)==0 && smpTvCrop(480,false)==16 && smpTvCrop(240,false)==8 && smpTvCrop(224,false)==0);
    require(smpTvSource(2048,448,true) && smpTvSource(512,480,false) && !smpTvSource(684,480,false) && !smpTvSource(512,480,true));
    bool rejected=false;try{smpGuiTvSurface(native.data(),stride*4,256,480,wide,out);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    std::vector<double> tvTime,legacyTime;volatile uint32_t checksum=0;
    for(unsigned i=0;i<210;i++) {
      native[16*stride+(i%512)]^=1;
      auto a=std::chrono::steady_clock::now();smpGuiTvSurface(native.data(),stride*4,512,480,wide,out);auto b=std::chrono::steady_clock::now();
      auto old=smpGuiSurface(native.data(),stride*4,512,480,wide);auto c=std::chrono::steady_clock::now();
      checksum=checksum^out[256+(i%512)*3]^old[16*684+86+(i%512)];
      if(i>=10){tvTime.push_back(std::chrono::duration<double,std::milli>(b-a).count());legacyTime.push_back(std::chrono::duration<double,std::milli>(c-b).count());}
    }
    std::sort(tvTime.begin(),tvTime.end());std::sort(legacyTime.begin(),legacyTime.end());
    std::cout<<"{\"passed\":true,\"native_center_samples_preserved\":229376,\"center_output_samples\":688128,\"displayed_center_pixels_equal\":7776000,\"active_lines\":224,\"margin_columns_retained\":86,\"tv_surface\":[2048,448],\"layout_checks\":[\"1920x1080\",\"3840x2160\",\"2560x1080\",\"800x600\"],\"conversion_ms\":{\"tv_median\":"<<tvTime[100]<<",\"tv_p95\":"<<tvTime[190]<<",\"tv_max\":"<<tvTime.back()<<",\"legacy_median\":"<<legacyTime[100]<<",\"legacy_p95\":"<<legacyTime[190]<<"},\"scope\":\"Data/layout/microbenchmark only; not end-to-end TV latency or pacing\"}\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
