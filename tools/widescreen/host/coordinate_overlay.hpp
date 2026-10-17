// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
// Retain only the panel's rows on compositor-owned surfaces. Native surfaces
// already use a separate overlay copy. Reset at each new emulated frame so an
// older underlay can never overwrite newly composed room pixels.
class SmpCoordinateUnderlay {
  uint32_t* owner=nullptr;
  unsigned width=0,height=0,top=0,rows=0;
  std::vector<uint32_t> pixels;
public:
  void reset(){owner=nullptr;}
  void capture(uint32_t* data,unsigned w,unsigned h,unsigned crop) {
    reset();if(!data || w<128 || h<64 || crop>=h/2)return;
    unsigned sy=h>=448?2:1;
    owner=data;width=w;height=h;top=h-crop-19*sy;rows=18*sy;
    pixels.assign(data+size_t(top)*w,data+size_t(top+rows)*w);
  }
  void restore(uint32_t* data,unsigned w,unsigned h) {
    if(owner && owner==data && width==w && height==h)
      std::copy(pixels.begin(),pixels.end(),data+size_t(top)*w);
    reset();
  }
};
inline std::array<uint8_t,5> smpCoordinateGlyph(char c) {
  static constexpr uint8_t glyphs[][5]={
    {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
    {54,73,73,73,54},{6,73,73,41,30},
    {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
    {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
    {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
    {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
    {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
    {3,4,120,4,3},{97,81,73,69,67}};
  int i=c>='0'&&c<='9'?c-'0':c>='A'&&c<='Z'?10+c-'A':-1;
  if(i>=0)return {glyphs[i][0],glyphs[i][1],glyphs[i][2],glyphs[i][3],glyphs[i][4]};
  if(c==':')return {0,0,36,0,0};if(c==',')return {0,64,32,0,0};
  if(c=='.')return {0,96,96,0,0};
  if(c=='-')return {8,8,8,8,8};return {};
}
inline void smpDrawCoordinatePanel(uint32_t* pixels,unsigned width,unsigned height,unsigned crop,
    const std::string& line1,const std::string& line2) {
  if(!pixels || width<128 || height<64 || crop>=height/2)return;
  unsigned sx=width==2048?6:width>=512?2:1,sy=height>=448?2:1;
  unsigned left=3*sx+(width==2048 || (width==684 && height==224)?1:0),top=height-crop-19*sy;
  unsigned panelWidth=unsigned(std::max(line1.size(),line2.size())*6+4)*sx;
  for(unsigned y=top;y<top+18*sy && y<height;y++)for(unsigned x=left;x<left+panelWidth && x<width;x++)pixels[y*width+x]=0x101018;
  const std::string* lines[]={&line1,&line2};
  for(unsigned n=0;n<2;n++)for(unsigned k=0;k<lines[n]->size();k++) {
    auto g=smpCoordinateGlyph((*lines[n])[k]);
    for(unsigned x=0;x<5;x++)for(unsigned y=0;y<7;y++)if(g[x]&(1u<<y))
      for(unsigned dx=0;dx<sx;dx++)for(unsigned dy=0;dy<sy;dy++) {
        unsigned px=left+(2+k*6+x)*sx+dx,py=top+(1+n*9+y)*sy+dy;
        if(px<width && py<height)pixels[py*width+px]=n?0xffffff:0xffff70;
      }
  }
}
inline const uint32_t* smpCoordinateSurface(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,unsigned crop,
    const std::string& line1,const std::string& line2,std::vector<uint32_t>& out) {
  if(!data || pitch<width*4 || width<128 || height<64 || crop>=height/2)return data;
  out.resize(size_t(width)*height);
  for(unsigned y=0;y<height;y++)std::copy_n(data+y*(pitch/4),width,out.data()+y*width);
  smpDrawCoordinatePanel(out.data(),width,height,crop,line1,line2);
  return out.data();
}
