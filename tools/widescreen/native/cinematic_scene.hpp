// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "registers.hpp"
#include "eye_state.hpp"
#include "cinematic_stars.hpp"
#include <algorithm>
#include <array>
#include <vector>
namespace SMPWide {
// Cinematics own their PPU maps; room memory is stale after the escape fade.
// Extend those actual maps and OAM at the raster boundary, preserving the
// original affine transform, palette changes, compositing, and guest timing.
class CinematicRow {
  Bytes v,p;
  Registers r;
  unsigned y;
  unsigned scene;
  std::optional<EyeRow> effect;
  bool repeatClouds=false,repeatCeresStars=false,repeatZebesStars=false,parkedCeresShip=false,singleZebesExplosion=false;
  struct Sample {unsigned color=0,priority=0,layer=3,palette=0;};
  struct Object {int x;unsigned row,size,attr;bool marginOnly=false;};
  std::vector<Object> objects;
  unsigned get(Reg k) const {return r.get(k);}
  unsigned color(unsigned index) const {return word(p,index*2)&32767;}
  unsigned texel(unsigned address,unsigned px,unsigned py,unsigned bpp) const {
    unsigned index=0;
    for(unsigned bit=0;bit<bpp;bit++)index|=((v[(address+(bit/2)*16+py*2+bit%2)&65535]>>(7-px))&1)<<bit;
    return index;
  }
  Sample background(unsigned layer,int x) const {
    const Reg bases[]={Reg::tiledataAddress,Reg::bg2_tiledataAddress,Reg::bg3_tiledataAddress};
    auto bg=[&](unsigned i){return get(Reg(unsigned(bases[layer])+i));};
    // These are the credits, closing captions, Samus art and final emblem.
    // Their 32-column maps are single compositions, not repeating scenery.
    if(scene!=4 && (x<0 || x>=256) && layer<2 && bg(1)>=0x4800 && bg(1)<=0x5400)return {};
    // The ending explosion's two32-column maps are one authored image.
    // Keep the native backdrop/color math in the margins without wrapping
    // the expanding rings back onto both edges of the screen.
    if(singleZebesExplosion && (x<0 || x>=256) && layer<2) {
      // 8B:F2B7 selects the initial wide flash (7000/7C00). Its horizontal
      // beam reaches both native edges; continue those edge texels through
      // the margins with the same raster scroll, palette and color math.
      // F2FA switches to the self-contained concentric rings (7400/7800),
      // which must remain a single image without repetition or stretching.
      if(get(Reg::screenAddress)==0x7000 && !get(Reg::screenSize) && !get(Reg::bg2_screenSize))
        x=std::clamp(x,0,255);
      else return {};
    }
    // Ceres captions and the Zebes planet are single compositions as well.
    if(scene==2 && (x<0 || x>=256) && layer<2 && bg(1)==0x5c00)return {};
    if(scene>=3 && layer==2 && (x<0 || x>=256))return {};
    // The floor IRQ switches to BG2, whose native map starts at scroll100h.
    // BG1 still contains Ridley's affine scroll; it cannot supply the floor.
    if(scene==4 && layer==1 && y>=208 && x>=256) x=224+(x-256)%16;
    unsigned sampleY=y;
    if(bg(11)) {
      int size=int(get(Reg::mosaicSize));x-=((x%size)+size)%size;
      sampleY-=get(Reg::mosaicSize)-get(Reg::mosaicCounter);
    }
    unsigned bpp=layer==2?2:4,xx=unsigned(x+int(bg(9))),yy=sampleY+bg(10);
    unsigned tx=(xx/8)&(bg(2)&1?63:31),ty=(yy/8)&(bg(2)&2?63:31);
    unsigned screen=tx/32+(ty/32)*(bg(2)&1?2:1);
    unsigned entry=word(v,((bg(1)+screen*1024+(ty%32)*32+tx%32)&32767)*2);
    unsigned px=xx&7,py=yy&7;
    if(entry&0x4000)px=7-px;if(entry&0x8000)py=7-py;
    unsigned index=texel(bg(0)*2+(entry&1023)*(8*bpp),px,py,bpp);
    return {color(((entry>>10)&7)*(1<<bpp)+index),index?bg(entry&0x2000?6:5):0,layer,0};
  }
  Sample affine(int x) const {
    // Retail parks the flyby ship just outside its256px view while the planet
    // caption waits. Don't reveal that parked edge early in the added margin.
    if(parkedCeresShip && (x<0 || x>=256))return {};
    auto signed13=[](int n){return (n&4096)?(n&8191)-8192:n&8191;};
    auto clip=[](int n){return n&8192?n|~1023:n&1023;};
    int a=int16_t(get(Reg::m7a)),b=int16_t(get(Reg::m7b)),c=int16_t(get(Reg::m7c)),d=int16_t(get(Reg::m7d));
    int cx=signed13(get(Reg::m7x)),cy=signed13(get(Reg::m7y));
    int h=clip(signed13(get(Reg::hoffsetMode7))-cx),vv=clip(signed13(get(Reg::voffsetMode7))-cy);
    int yy=get(Reg::vflipMode7)?255-int(y):int(y);
    if(get(Reg::hflipMode7))x=255-x;
    int px=((a*h&~63)+(b*vv&~63)+(b*yy&~63)+cx*256+a*x)>>8;
    int py=((c*h&~63)+(d*vv&~63)+(d*yy&~63)+cy*256+c*x)>>8;
    bool outside=px<0 || px>=1024 || py<0 || py>=1024;
    unsigned repeat=get(Reg::repeatMode7),index=0;
    if(!outside || repeat<2 || repeat==3) {
      unsigned tile=outside && repeat==3?0:v[2*((unsigned(py)&1023)/8*128+(unsigned(px)&1023)/8)];
      index=v[2*(tile*64+(unsigned(py)&7)*8+(unsigned(px)&7))+1];
    }
    return {color(index),index?get(Reg::priority0):0,0,0};
  }
  Sample object(int x) const {
    for(const auto& q:objects) {
      if(q.marginOnly && x>=0 && x<256)continue;
      int sampleX=x;
      // 8C:96CB/975E/979C/97BC/97D2 contain these star glyphs. The planet and
      // caption use different maps; keep both single, while continuing stars.
      bool ceresStar=repeatCeresStars && (q.attr==0x0800 || q.attr==0x0808 || q.attr==0x0809 ||
        q.attr==0x0840 || q.attr==0x0850 || q.attr==0x08ab);
      bool purpleStar=repeatZebesStars && (q.attr==0x0eed || (q.attr>=0x0ef8 && q.attr<=0x0efc));
      bool repeat=(repeatClouds || ceresStar || purpleStar) && (x<0 || x>=256);
      if(repeat) {
        while(sampleX<q.x)sampleX+=256;
        while(sampleX>=q.x+256)sampleX-=256;
      }
      if(sampleX<q.x || sampleX>=q.x+int(q.size))continue;
      unsigned xx=sampleX-q.x,yy=q.row;
      if(q.attr&0x4000)xx=q.size-1-xx;if(q.attr&0x8000)yy=q.size-1-yy;
      unsigned tile=(q.attr&256)|(((q.attr&0xf0)+(yy/8)*16)&0xf0)|(((q.attr&15)+xx/8)&15);
      unsigned address=get(Reg::obj_tiledataAddress)*2+(tile&255)*32;
      if(tile&256)address+=(get(Reg::obj_nameselect)+1)*8192;
      unsigned index=texel(address,xx&7,yy&7,4);
      if(index){unsigned pal=(q.attr>>9)&7;return {color(128+pal*16+index),get(Reg(unsigned(Reg::obj_priority0)+((q.attr>>12)&3))),4,pal};}
    }
    return {};
  }
public:
  CinematicRow(Bytes vram,Bytes palette,Bytes oam,Bytes registers,unsigned line,unsigned function,unsigned kind=1,std::optional<EyeRow> eye={},std::span<const CinematicStar> stars={}):v(vram),p(palette),r(registers),y(line),scene(kind),effect(std::move(eye)) {
    // The second fly-through uses a tiled OBJ cloud sheet over Mode7 clouds.
    // Continue that sheet too, rather than exposing its darker lower plane.
    repeatClouds=scene==1 && get(Reg::bgMode)==7 && get(Reg::obj_tiledataAddress)==0x6000 &&
      (function==0xd7f8 || function==0xd807 || function==0xd837);
    repeatCeresStars=scene==2 && get(Reg::bgMode)==7 && get(Reg::m7x)==56 && get(Reg::m7y)==24;
    parkedCeresShip=repeatCeresStars && function==0xa38f;
    // 8C:A28B is the static purple starfield, separate from planet/glow art.
    repeatZebesStars=scene==1 && get(Reg::obj_tiledataAddress)==0x4000 &&
      (function==0xda02 || function==0xda1a || function==0xdad3 || function==0xdb9d || function==0xdb9e || function==0xdbc4);
    singleZebesExplosion=scene==1 && get(Reg::bgMode)==1 && get(Reg::tiledataAddress)==0x4000 &&
      ((get(Reg::screenAddress)==0x7000 && get(Reg::bg2_screenAddress)==0x7c00) ||
       (get(Reg::screenAddress)==0x7400 && get(Reg::bg2_screenAddress)==0x7800));
    static const unsigned small[]={8,8,8,16,16,32,16,16},large[]={16,32,64,32,64,64,32,32};
    for(unsigned n=0;n<128;n++) {
      unsigned i=(get(Reg::obj_firstSprite)+n)&127,high=oam[512+i/4]>>(i%4*2);
      unsigned size=(high&2)?large[get(Reg::obj_baseSize)]:small[get(Reg::obj_baseSize)];
      unsigned row=(y-1-oam[i*4+1])&255;if(row>=size)continue;
      int x=oam[i*4]|((high&1)<<8);if(x>=342)x-=512;
      objects.push_back({x,row,size,word(oam,i*4+2)});
    }
    if((scene==1 && get(Reg::obj_tiledataAddress)==0x6000) || scene==4)for(const auto& star:stars) {
      int row=int(y)-1-star.y;
      if(row>=0 && row<int(star.size))objects.push_back({star.x,unsigned(row),star.size,star.attr,true});
    }
  }
  bool supported() const {
    if(get(Reg::displayDisable))return true;
    if((get(Reg::bgMode)!=1 && get(Reg::bgMode)!=7) || get(Reg::extbg) || get(Reg::pseudoHires) || get(Reg::interlace) ||
       get(Reg::screen_directColor) ||
       get(Reg::tileSize) || get(Reg::bg2_tileSize) || get(Reg::bg3_tileSize) || get(Reg::obj_interlace))return false;
    if((get(Reg::mosaicEnable) || get(Reg::bg2_mosaicEnable) || get(Reg::bg3_mosaicEnable)) &&
       (!get(Reg::mosaicSize) || get(Reg::mosaicSize)>16 || get(Reg::mosaicCounter)>get(Reg::mosaicSize) ||
        (get(Reg::bgMode)==7 && get(Reg::mosaicSize)!=1)))return false;
    const Reg windows[]={Reg::window_bg1_oneEnable,Reg::window_bg2_oneEnable,Reg::window_bg3_oneEnable,
      Reg::window_obj_oneEnable,Reg::window_col_oneEnable};
    for(auto key:windows)if(get(key))return false;
    const Reg twos[]={Reg::window_bg1_twoEnable,Reg::window_bg2_twoEnable,Reg::window_bg3_twoEnable,
      Reg::window_obj_twoEnable,Reg::window_col_twoEnable};
    for(auto key:twos)if(get(key) && (!effect || !effect->powerBomb))return false;
    return true;
  }
  bool masked(unsigned layer,int x) const {
    if(!effect)return false;
    Reg base=layer==4?Reg::window_obj_twoEnable:Reg(unsigned(Reg::window_bg1_twoEnable)+layer*7);
    return get(base) && effect->inside(x)!=bool(get(Reg(unsigned(base)+1)));
  }
  unsigned pixel(int x) const {
    if(get(Reg::displayDisable))return 0;
    if(scene==4 && y>=32 && (x<0 || x>=336))return 0;
    Sample main{color(0),0,3,0},sub=main;
    const Reg above[]={Reg::aboveEnable,Reg::bg2_aboveEnable,Reg::bg3_aboveEnable};
    const Reg below[]={Reg::belowEnable,Reg::bg2_belowEnable,Reg::bg3_belowEnable};
    for(unsigned i=0;i<(get(Reg::bgMode)==7?1u:3u);i++) {
      if(!get(above[i]) && !get(below[i]))continue;
      auto s=get(Reg::bgMode)==7?affine(x):background(i,x);
      bool mask=masked(i,x);
      if(get(above[i]) && !(mask && get(Reg(unsigned(Reg::window_bg1_aboveEnable)+i*7))) && s.priority>main.priority)main=s;
      if(get(below[i]) && !(mask && get(Reg(unsigned(Reg::window_bg1_belowEnable)+i*7))) && s.priority>sub.priority)sub=s;
    }
    if(get(Reg::obj_aboveEnable) || get(Reg::obj_belowEnable)) {
      auto s=object(x);
      bool mask=masked(4,x);
      if(get(Reg::obj_aboveEnable) && !(mask && get(Reg::window_obj_aboveEnable)) && s.priority>main.priority)main=s;
      if(get(Reg::obj_belowEnable) && !(mask && get(Reg::window_obj_belowEnable)) && s.priority>sub.priority)sub=s;
    }
    unsigned result=main.color;
    const Reg math[]={Reg::screen_bg1_colorEnable,Reg::screen_bg2_colorEnable,Reg::screen_bg3_colorEnable,Reg::screen_back_colorEnable,Reg::screen_obj_colorEnable};
    bool aboveColor=true,belowColor=true;
    {
      bool inside=effect && get(Reg::window_col_twoEnable) && effect->inside(x)!=bool(get(Reg::window_col_twoInvert));
      bool modes[]={true,inside,!inside,false};
      aboveColor=modes[get(Reg::window_col_aboveMask)];belowColor=modes[get(Reg::window_col_belowMask)];
      if(!aboveColor)result=0;
    }
    if(belowColor && get(math[main.layer]) && (main.layer!=4 || main.palette>=4)) {
      bool useSub=get(Reg::screen_blendMode) && sub.priority;
      unsigned second=useSub?sub.color:(get(Reg::screen_colorRed)|(get(Reg::screen_colorGreen)<<5)|(get(Reg::screen_colorBlue)<<10));
      unsigned first=result;result=0;
      for(unsigned sh:{0,5,10}) {
        int a=(first>>sh)&31,b=(second>>sh)&31,n=get(Reg::screen_colorMode)?std::max(0,a-b):a+b;
        if(get(Reg::screen_colorHalve) && aboveColor && (!get(Reg::screen_blendMode) || useSub))n/=2;
        result|=std::min(31,n)<<sh;
      }
    }
    unsigned rgb=0;
    for(unsigned i=0;i<3;i++)rgb|=((((result>>(i*5))&31)*get(Reg::displayBrightness)*16+15)/30)<<(16-8*i);
    return rgb;
  }
};
inline bool cinematicRow(Bytes v,Bytes p,Bytes o,Bytes regs,unsigned line,unsigned function,uint8_t* out,unsigned scene=1,std::optional<EyeRow> effect={},std::span<const CinematicStar> stars={}) {
  if(v.size()!=65536 || p.size()!=512 || o.size()!=544 || regs.size()!=205 || line<1 || line>224 || !out)return false;
  CinematicRow row(v,p,o,regs,line,function,scene,std::move(effect),stars);if(!row.supported())return false;
  bool arena=scene==4 && line>=32;
  for(int x=arena?0:-43;x<(arena?342:299);x++) {
#ifndef SMP_CINEMATIC_VALIDATE_CENTER
    if(x>=0 && x<256)continue;
#endif
    unsigned rgb=row.pixel(x);
    for(unsigned i=0;i<3;i++)out[((line-1)*342+x+(arena?0:43))*3+i]=rgb>>(16-8*i);
  }
  return true;
}
}

