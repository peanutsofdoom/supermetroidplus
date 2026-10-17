// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
// Only for the owned canonical TV surface: its half-pixels repeat three times
// horizontally (with two outer samples cropped), and rows repeat twice.
// The matching nearest GPU decoder samples canonical coordinates BEFORE
// looking up packed texels, retaining the exact16:9 crop and pixel boundaries.
inline void smpPackTvUpload(const uint32_t* data,unsigned pitch,std::vector<uint32_t>& packed) {
  packed.resize(684*224);
  for(unsigned y=0;y<224;y++) {
    const auto* row=data+(y*2)*(pitch/4);
    for(unsigned x=0;x<684;x++)packed[y*684+x]=row[x?x*3-2:0];
  }
}
inline void smpUnpackTvUpload(const uint32_t* data,unsigned pitch,std::vector<uint32_t>& canonical) {
  canonical.resize(2048*448);
  for(unsigned y=0;y<448;y++)for(unsigned x=0;x<2048;x++)
    canonical[y*2048+x]=data[(y/2)*(pitch/4)+(x+2)/3];
}
