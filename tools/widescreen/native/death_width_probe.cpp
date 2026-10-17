// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>&b,Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=n>>(8*i);}
struct Empty {Pixel pixel(int,int) const {return {};}};
int main(){try {
 for(unsigned state=21;state<=25;state++)if(gameplayPresentationState(state))throw std::runtime_error("Late death must not revive world actors");
 std::vector<uint8_t> v(65536),p(512),r(205);Empty g;
 put(r,Reg::bgMode,1);put(r,Reg::displayBrightness,15);
 p[0]=255;p[1]=127;
 Mode1 backdrop(g,v,p,r,100,0,0);
 for(int x=-43;x<299;x++){auto a=backdrop.pixel(x);if(!a.valid || a.rgb!=std::array<uint8_t,3>{248,248,248})throw std::runtime_error("Death backdrop clipped to room bounds");}
 put(r,Reg::bg2_aboveEnable,1);put(r,Reg::bg2_priority0,1);put(r,Reg::bg2_priority1,2);put(r,Reg::bg2_screenAddress,0x1000);put(r,Reg::bg2_mosaicEnable,1);
 // Single tile, color varying by row and column. Independent expected fetch.
 for(int y=0;y<8;y++)for(int x=0;x<8;x++){unsigned color=1+((x+3*y)%15);for(int bit=0;bit<4;bit++)v[(bit/2)*16+y*2+bit%2]|=((color>>bit)&1)<<(7-x);}
 for(int c=1;c<16;c++)p[c*2]=c;
 unsigned checks=0;
 for(unsigned size=1;size<=16;size++)for(unsigned phase=0;phase<size;phase++){
  put(r,Reg::mosaicSize,size);put(r,Reg::mosaicCounter,size-phase);
  Mode1 row(g,v,p,r,64,0,0);
  for(int x=-43;x<299;x++){
   int anchor=x;while(((anchor%int(size))+int(size))%int(size))--anchor;
   unsigned color=1+((unsigned(anchor)&7)+3*((64-phase)&7))%15;
   auto a=row.pixel(x);if(!a.valid || a.rgb[0]!=color*8 || a.rgb[1] || a.rgb[2])throw std::runtime_error("Mosaic fetch differs");checks++;
  }
 }
 put(r,Reg::bg2_aboveEnable,0);put(r,Reg::bg2_mosaicEnable,0);
 std::vector<uint8_t> ram(131072);ram[0x998]=21;ram[0x7a5]=16;ram[0x7a7]=16;
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 for(unsigned line=1;line<32;line++){
  RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,line);row.finish();
  for(int x:{-43,298})if(row.pixel(x).rgb!=std::array<uint8_t,3>{248,248,248})throw std::runtime_error("Death upper corner mismatch");
 }
 ram[0x998]=8;owner=std::make_shared<const OwnerImage>(2,ram);scene={owner,owner,{},SceneKind::Ordinary};
 bool rejected=false;try{RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,1);}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("Gameplay HUD guard weakened");
 std::cout<<"PASS "<<checks<<" BG2 mosaic samples including both margins and all phases; death backdrop fills canvas\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
