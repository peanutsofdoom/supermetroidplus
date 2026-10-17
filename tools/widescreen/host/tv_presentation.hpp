// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bridge.h"
#include "native_edge_patch.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>

struct SMPTvSize {unsigned width,height;};
inline SMPTvSize smpTvFit(unsigned width,unsigned height,bool wide) {
  if(!width || !height)return {0,0};
  const unsigned numerator=wide?16:4,denominator=wide?9:3;
  if(uint64_t(width)*denominator<=uint64_t(height)*numerator)
    return {width,unsigned(uint64_t(width)*denominator/numerator)};
  return {unsigned(uint64_t(height)*numerator/denominator),height};
}
inline bool smpTvSource(unsigned width,unsigned height,bool wide) {
  if(wide)return (width==2048 && height==448) || (width==684 && height==224);
  return (width==256 || width==512) && (height==224 || height==240 || height==448 || height==480);
}
inline unsigned smpTvCrop(unsigned height,bool wide) {
  if(wide)return 0;
  return height==480?16:height==240?8:0;
}

// 342 logical columns cover an ideal341+1/3-column16:9 field around a4:3
// 256-column center. A3x horizontal lattice keeps every native sample exact:
// 684*3=2052, minus2 outer lattice samples on each edge =2048. The center is
// exactly1536/2048=3/4 of the display. No center or active vertical row is lost.
inline void smpGuiTvSurface(const uint32_t* native,unsigned pitch,unsigned width,unsigned height,SMPWideOutput wide,std::vector<uint32_t>& output) {
  if(!native || width!=512 || height!=480 || pitch<512*4 || pitch%4 ||
     !wide.data || wide.size!=342*224*3 || wide.width!=342 || wide.height!=224)
    throw std::invalid_argument("TV surface geometry");
  output.resize(2048*448);
  for(unsigned y=0;y<448;y++) {
    auto* row=output.data()+y*2048;
    const auto* center=native+(y+16)*(pitch/4);
    // Non-interlaced native rows are normally duplicates. The margin and
    // repair mask share the same logical row too; retain a checked fallback.
    if((y&1) && std::equal(center,center+512,center-pitch/4)) {
      std::copy(row-2048,row,row);continue;
    }
    if(!wide.center_mask_size) {
      for(unsigned x=0;x<512;x++)row[256+x*3]=row[257+x*3]=row[258+x*3]=center[x];
    } else {
      for(unsigned x=0;x<512;x++)row[256+x*3]=row[257+x*3]=row[258+x*3]=smpNativeEdgeSample(center,x,y/2,wide);
    }
    for(unsigned x=0;x<256;x++) {
      auto rgb=[&](unsigned logical) {
        const auto* p=wide.data+((y/2)*342+logical)*3;
        return uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];
      };
      row[x]=rgb((x+2)/6);
      row[1792+x]=rgb((1794+x)/6);
    }
  }
}
// Produce exactly the texels sampled by smpPackTvUpload without materializing
// the 3x-by-2x CPU surface first. The GPU retains the canonical crop lattice.
inline void smpGuiCompactTvSurface(const uint32_t* native,unsigned pitch,unsigned width,unsigned height,SMPWideOutput wide,std::vector<uint32_t>& output) {
  if(!native || width!=512 || height!=480 || pitch<512*4 || pitch%4 ||
     !wide.data || wide.size!=342*224*3 || wide.width!=342 || wide.height!=224)
    throw std::invalid_argument("compact TV surface geometry");
  output.resize(684*224);
  for(unsigned y=0;y<224;y++) {
    auto* row=output.data()+y*684;
    const auto* center=native+(y*2+16)*(pitch/4);
    if(!wide.center_mask_size)std::copy(center,center+512,row+86);
    else for(unsigned x=0;x<512;x++)row[86+x]=smpNativeEdgeSample(center,x,y,wide);
    for(unsigned x=0;x<86;x++) {
      auto rgb=[&](unsigned column){auto* p=wide.data+(y*342+column)*3;return uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];};
      row[x]=rgb(x/2);row[598+x]=rgb((598+x)/2);
    }
  }
}
