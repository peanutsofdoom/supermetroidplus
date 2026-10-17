// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "title_subtitle_asset.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
// Presentation only. Native title timer controls repeatable reveal/restore;
// no ROM, emulated timing, input or framebuffer writes.
inline double smpTitleEase(double t){t=std::clamp(t,0.0,1.0);return t*t*(3.0-2.0*t);}
inline int smpTitleAge(unsigned function,unsigned timer) {
  if(function!=0x9f29 && function!=0x9f52 && function!=0x9fae)return -1;
  return timer>900?900:900-int(timer);
}
struct SmpTitleSkip {
  bool skipped=false;
  void reset(){skipped=false;}
  bool observe(unsigned game,unsigned function,unsigned transition) {
    if(game!=1 || function==0x9b68)reset();
    // Native skip transition 3 fades in the assembled final title image.
    if(game==1 && (transition==3 || function==0x9f52))skipped=true;
    return skipped;
  }
};
inline void smpTitleReveal(std::vector<uint32_t>& pixels,unsigned width,unsigned height,unsigned crop,
                           unsigned ageFunction,unsigned timer,unsigned brightness,bool skipped=false) {
  if(!width || pixels.size()!=size_t(width)*height || height<=crop*2)return;
  int age=skipped?900:smpTitleAge(ageFunction,timer);
  double reveal=smpTitleEase((age-45)/240.0);
  unsigned margin=width==2048?256:86,cut=unsigned(std::lround(margin*(1.0-reveal)));
  for(unsigned y=0;y<height;y++) {
    auto* row=pixels.data()+size_t(y)*width;
    std::fill(row,row+cut,0);std::fill(row+width-cut,row+width,0);
  }
  if(age<315 || brightness&128)return;
  double landing=std::clamp((age-315)/18.0,0.0,1.0);
  double opacity=std::min(1.0,landing*4.0);
  double scale=1.0;
  double cx=448.0,cy=212.0;
  unsigned active=height-2*crop;
  int x0=std::max(0,int((cx-SmpStampWidth*scale/2-3)*width/896));
  int x1=std::min(int(width),int((cx+SmpStampWidth*scale/2+3)*width/896)+1);
  int y0=std::max(int(crop),int(crop+(cy-SmpStampHeight*scale/2-3)*active/504));
  int y1=std::min(int(height-crop),int(crop+(cy+SmpStampHeight*scale/2+3)*active/504)+1);
  for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++) {
    double u=((x+0.5)*896/width-cx)/scale+SmpStampWidth/2.0-0.5;
    double v=((y-crop+0.5)*504/active-cy)/scale+SmpStampHeight/2.0-0.5;
    int ix=int(std::floor(u+0.5)),iy=int(std::floor(v+0.5));
    if(ix<0 || iy<0 || ix>=int(SmpStampWidth) || iy>=int(SmpStampHeight))continue;
    uint32_t source=SmpSubtitlePixels[iy*SmpStampWidth+ix];
    double alpha=(source>>24)/255.0*opacity;if(alpha<=0)continue;
    auto& dst=pixels[size_t(y)*width+x];unsigned result=dst&0xff000000;
    for(unsigned shift:{0u,8u,16u}) {
      double channel=((source>>shift)&255)*(brightness&15)/15.0;
      result|=unsigned(std::lround(((dst>>shift)&255)*(1-alpha)+channel*alpha))<<shift;
    }
    dst=result;
  }
}
