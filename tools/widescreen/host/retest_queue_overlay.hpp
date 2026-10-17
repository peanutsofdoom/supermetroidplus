// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "coordinate_overlay.hpp"
#include <cstdio>
// The panel fits inside the left HUD margin on widescreen frames.
inline void smpDrawQueuePanel(uint32_t* pixels,unsigned width,unsigned height,unsigned crop,const char* code,unsigned seconds,bool complete,bool error) {
  if(!pixels || width<128 || height<=2*crop+32 || !code || !code[0] || !code[1])return;
  unsigned sx=width==2048?6:width>=512?2:1,sy=height>=448?2:1;
  unsigned left=6*sx,top=crop+2*sy;
  for(unsigned y=top;y<top+28*sy && y<height;y++)for(unsigned x=left;x<left+30*sx && x<width;x++)pixels[y*width+x]=0x101018;
  auto text=[&](const char* s,unsigned x,unsigned y,unsigned scale,uint32_t color){
    for(unsigned k=0;s[k];k++){auto g=smpCoordinateGlyph(s[k]);for(unsigned a=0;a<5;a++)for(unsigned b=0;b<7;b++)if(g[a]&(1u<<b))for(unsigned dx=0;dx<sx*scale;dx++)for(unsigned dy=0;dy<sy*scale;dy++){
      unsigned xx=x+(k*6+a)*sx*scale+dx,yy=y+b*sy*scale+dy;
      if(xx<width && yy<height)pixels[yy*width+xx]=color;
    }}
  };
  char id[3]={code[0],code[1],0},timer[16];std::snprintf(timer,sizeof(timer),"%uS",seconds);
  text(id,left+3*sx,top+2*sy,2,0xffe070);
  text(error?"ERR":complete?"END":timer,left+6*sx,top+19*sy,1,error?0xff7070:0xffffff);
}
