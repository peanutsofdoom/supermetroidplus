// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bridge.h"
#include "native_edge_patch.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
inline std::vector<uint32_t> smpGuiSurface(const uint32_t* native,unsigned pitch,unsigned width,unsigned height,SMPWideOutput wide) {
  if(!native || width!=512 || height!=480 || pitch<512*4 || pitch%4 ||
     !wide.data || wide.size!=342*224*3 || wide.width!=342 || wide.height!=224)
    throw std::invalid_argument("GUI surface geometry");
  std::vector<uint32_t> output(684*480,0);
  for(unsigned y=0;y<480;y++)std::copy(native+y*(pitch/4),native+y*(pitch/4)+512,output.begin()+y*684+86);
  if(wide.center_mask_size)for(unsigned y=16;y<464;y++)for(unsigned x=0;x<512;x++)
    output[y*684+86+x]=smpNativeEdgeSample(native+y*(pitch/4),x,(y-16)/2,wide);
  for(unsigned y=0;y<224;y++)for(unsigned x=0;x<342;x++) {
    if(x>=43 && x<299)continue;
    auto p=&wide.data[(y*342+x)*3];uint32_t color=uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];
    unsigned yy=(y+8)*2;
    for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)output[(yy+dy)*684+x*2+dx]=color;
  }
  return output;
}
