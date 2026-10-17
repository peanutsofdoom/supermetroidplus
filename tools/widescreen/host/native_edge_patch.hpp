// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bridge.h"
#include <algorithm>
// Preserve native samples except explicitly qualified omitted enemy pixels
// and the adjacent half-sample required by the native blur filter.
inline uint32_t smpNativeEdgeSample(const uint32_t* row,unsigned sample,unsigned y,SMPWideOutput wide) {
  if(!wide.center_mask || wide.center_mask_size!=256*224)return row[sample];
  unsigned x=sample/2,next=wide.center_mask[y*256+x]==2?x+1:std::min(x+1,255u);
  auto changed=[&](unsigned q){return wide.center_mask[y*256+q]!=0;};
  bool half=(sample&1) && wide.blur;
  if(!changed(x) && !(half && changed(next)))return row[sample];
  const auto* a=wide.data+(y*342+x+43)*3;
  const auto* b=wide.data+(y*342+next+43)*3;
  unsigned color=row[sample]&0xff000000u;
  for(unsigned c=0;c<3;c++)color|=(half?(unsigned(a[c])+b[c])/2:a[c])<<(16-c*8);
  return color;
}
