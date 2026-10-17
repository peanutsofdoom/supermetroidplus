// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded Mode 1 composition. Native center copying and objects are host tasks.
#pragma once
#include "registers.hpp"
#include "eye_state.hpp"
#include "ship_objects.hpp"
#include "enemy_objects.hpp"
#include <algorithm>
#include <array>

namespace SMPWide {
struct RGBPixel {std::array<uint8_t,3> rgb{};bool valid=false;};
struct Bg3Fetch {unsigned until,screen,size;};
struct Layer {
  unsigned base,screen,size,large,mode,low,high,above,below,x,y,mosaic;
};
template<class Geometry> class Mode1 {
  const Geometry& geometry;
  Bytes vram,palette;
  Registers registers;
  std::array<Layer,3> layers;
  unsigned line;
  int cx,cy;
  bool blank;
  std::optional<EyeRow> eye;
  const ShipSampler* ship;
  const EnemySampler* enemies;
  const XrayTerrain* xrayTerrain;
  std::optional<Bg3Fetch> bg3Fetch;
  Pixel tile(const Layer& layer,int x,unsigned bpp) const {
    unsigned sampleLine=line;
    if(layer.mosaic) {
      int size=int(registers.get(Reg::mosaicSize));
      x-=((x%size)+size)%size;
      sampleLine-=registers.get(Reg::mosaicSize)-registers.get(Reg::mosaicCounter);
    }
    // Power-of-two wrapping applies only to the native repeating tilemap.
    unsigned tx=(unsigned(x+int(layer.x))/8)&(layer.size&1?63:31);
    unsigned ty=((sampleLine+layer.y)/8)&(layer.size&2?63:31);
    unsigned screen=tx/32+(ty/32)*(layer.size&1?2:1);
    auto entry=word(vram,((layer.screen+screen*1024+(ty%32)*32+tx%32)&32767)*2);
    unsigned px=unsigned(x+int(layer.x))&7,py=(sampleLine+layer.y)&7;
    if(entry&0x4000)px=7-px;
    if(entry&0x8000)py=7-py;
    unsigned address=layer.base*2+(entry&1023)*(8*bpp),index=0;
    for(unsigned p=0;p<bpp;p++)
      index|=((vram[(address+(p/2)*16+py*2+p%2)&65535]>>(7-px))&1)<<p;
    unsigned colorIndex=((entry>>10)&7)*(1<<bpp)+index;
    return {uint16_t(word(palette,colorIndex*2)&32767),uint8_t((entry>>13)&1),bool(index),true};
  }
public:
  Mode1(const Geometry& g,Bytes v,Bytes p,Bytes r,unsigned scanline,int cameraX,int cameraY,std::optional<EyeRow> effect={},const ShipSampler* object=nullptr,std::optional<Bg3Fetch> fetched={},const EnemySampler* enemyObjects=nullptr,const XrayTerrain* xrayMap=nullptr):
    geometry(g),vram(v),palette(p),registers(r),line(scanline),cx(cameraX),cy(cameraY),eye(std::move(effect)),ship(object),enemies(enemyObjects),xrayTerrain(xrayMap),bg3Fetch(fetched) {
    if(v.size()!=65536 || p.size()!=512)throw std::invalid_argument("Mode 1 image sizes");
    auto get=[&](Reg key){return registers.get(key);};
    if(eye && (eye->line!=int(line) || eye->left!=int(get(eye && (eye->powerBomb || eye->xray)?Reg::window_twoLeft:Reg::window_oneLeft)) || eye->right!=int(get(eye && (eye->powerBomb || eye->xray)?Reg::window_twoRight:Reg::window_oneRight))))
      throw std::invalid_argument("eye descriptor does not match raster");
    blank=get(Reg::displayDisable);
    const Reg bases[]={Reg::tiledataAddress,Reg::bg2_tiledataAddress,Reg::bg3_tiledataAddress};
    for(unsigned i=0;i<3;i++) {
      unsigned a[12];for(unsigned n=0;n<12;n++)a[n]=get(Reg(unsigned(bases[i])+n));
      layers[i]={a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10],a[11]};
    }
    if(blank)return;
    if(get(Reg::bgMode)!=1 || get(Reg::pseudoHires) || get(Reg::interlace))
      throw std::invalid_argument("unsupported Mode 1 display state");
    for(unsigned i=0;i<3;i++)if(layers[i].large || (layers[i].mosaic && i!=1))
      throw std::invalid_argument("unsupported tile size or mosaic");
    if(layers[1].mosaic && (!get(Reg::mosaicSize) || get(Reg::mosaicSize)>16 || get(Reg::mosaicCounter)>get(Reg::mosaicSize)))
      throw std::invalid_argument("invalid mosaic phase");
    const Reg windows[]={Reg::window_bg1_oneEnable,Reg::window_bg1_twoEnable,
      Reg::window_bg2_oneEnable,Reg::window_bg2_twoEnable,Reg::window_bg3_oneEnable,Reg::window_bg3_twoEnable,
      Reg::window_obj_oneEnable,Reg::window_obj_twoEnable,Reg::window_col_oneEnable,Reg::window_col_twoEnable,
      Reg::window_col_aboveMask,Reg::window_col_belowMask};
    for(auto key:windows)if(get(key)) {
      if(eye && (eye->powerBomb || eye->xray) && (key==Reg::window_bg1_twoEnable || key==Reg::window_bg2_twoEnable || key==Reg::window_bg3_twoEnable || key==Reg::window_obj_twoEnable || key==Reg::window_col_twoEnable))continue;
      if(eye && !eye->powerBomb && !eye->xray && (key==Reg::window_bg3_oneEnable || key==Reg::window_col_oneEnable))continue;
      if(eye && (eye->suit || eye->powerBomb || eye->xray || eye->motherBrain) && get(key)<=3 && (key==Reg::window_col_aboveMask || key==Reg::window_col_belowMask))continue;
      throw std::invalid_argument("unsupported window extension");
    }
  }
  bool terrainEdgeCandidate(int x) const {
    if constexpr(requires {geometry.wideBossEdgeCandidate(x,cx,layers[1].x);})
      if(geometry.wideBossEdgeCandidate(x,cx,layers[1].x))return true;
    if constexpr(requires {geometry.quakeEdgeCandidate(x,cx);})return geometry.quakeEdgeCandidate(x,cx);
    return false;
  }
  bool stagedCrocomire() const {
    if constexpr(requires {geometry.stagedCrocomire();})return geometry.stagedCrocomire();
    return false;
  }
  bool stagingCandidate(int x) const {
    if(!stagedCrocomire())return false;
    if constexpr(requires {geometry.crocomireWall(cx+x,cy+int(line));})
      if(geometry.crocomireWall(cx+x,cy+int(line)))return true;
    // The cleared boss BG2 tilemap can still sample nonblank tile0 as sprite
    // graphics are reused. These old native pixels need the same witnessed
    // replacement as the relocated wall and actor.
    if((layers[1].above || layers[1].below) && tile(layers[1],x,4).opaque)return true;
    return enemies && enemies->stagingCandidate(x,line);
  }
  bool ceresCaptionAt(int x) const {
    if constexpr(requires {geometry.ceresArena();}) {
      if(!geometry.ceresArena() || layers[0].screen!=0x5000 || layers[0].base!=0x6000 || layers[0].mosaic || x<0 || x>=256)return false;
      unsigned xx=unsigned(x+int(layers[0].x)),yy=line+layers[0].y;
      unsigned tx=(xx/8)&63,ty=(yy/8)&31;
      auto entry=word(vram,(0x5000+(tx/32)*1024+ty*32+tx%32)*2);
      // A6:C164/C450 write these glyphs directly to BG1, outside level RAM.
      bool english=(ty==6 && tx>=11 && tx<=19 && entry>=0x3982 && entry<=0x399b) ||
        ((ty==8 || ty==10 || ty==12) && tx>=5 && tx<=26 && entry>=0x3582 && entry<=0x359c);
      bool japanese=ty>=20 && ty<=23 && tx>=10 && tx<=21 && entry>=0x3d9c && entry<=0x3dc3;
      return english || japanese;
    }
    return false;
  }
  bool ceresUICandidate(int x) const {
    if constexpr(requires {geometry.ceresArena();})
      if(geometry.ceresArena())return ceresCaptionAt(x) || ceresCaptionAt(x-43) ||
        (enemies && (enemies->ceresTimerAt(x,line) || enemies->ceresTimerAt(x,line,43)));
    return false;
  }
  RGBPixel pixel(int x,bool fillNative=false,int ceresUIShift=-1) const {
    if(blank)return {{0,0,0},true};
    // Match the already-black out-of-room margin at the shaken room edge.
    // The native-column repair is still gated by exact background matching
    // and native-object exclusion in the frame pipeline.
    if constexpr(requires {geometry.quakeOutsideRoom(x,cx);})
      if(fillNative && !eye && geometry.quakeOutsideRoom(x,cx))return {{0,0,0},true};
    if constexpr(requires {geometry.ceresFloorOutside(x,layers[1].x);})
      if(line>=208 && !layers[0].above && !layers[0].below &&
         (layers[1].above || layers[1].below) && layers[1].screen==0x4800 &&
         geometry.ceresFloorOutside(x,layers[1].x))return {{0,0,0},true};
    struct Screen {unsigned priority,color,layer;};
    Screen main{0,unsigned(word(palette,0)&32767),3},sub=main;
    const bool edge=terrainEdgeCandidate(x),staged=stagedCrocomire();
    const bool nativeRing=!fillNative && (edge || (staged && x>=0 && x<256));
    // Reference pixels use the native ring. Do not decode room terrain first
    // when that result would immediately be replaced by the ring sample.
    auto terrain=nativeRing?tile(layers[0],x,4):geometry.pixel(cx+x,cy+int(line));
    if constexpr(requires {geometry.unusedPadding(cx+x,cy+int(line));}) {
      if((x<0 || x>=256) && geometry.unusedPadding(cx+x,cy+int(line))) {
        if(!(eye && (eye->suit || eye->powerBomb || eye->xray || eye->motherBrain)))return {{0,0,0},true};
        terrain={}; // Preserve existing full-width effect math over black padding.
      }
    }
    // The reference must use the actual native tile ring, including its exact
    // quake/scroll phase. Candidate terrain is the full-room relocated wall.
    if constexpr(requires {geometry.crocomireWall(cx+x,cy+int(line));}) {
      if(fillNative || x<0 || x>=256)if(auto wall=geometry.crocomireWall(cx+x,cy+int(line)))terrain=*wall;
    }
    // At the native seam a quake can expose an unpopulated VRAM ring column.
    // The caller accepts the full-room replacement only when this exact ring
    // reference matches the native pixel; objects/effects therefore stay safe.
    if(ceresUIShift>=0) {
      // Reference the actual native tile ring, then move only qualified UI
      // glyphs. Rebuild vacated text cells from the room's underlying air.
      if(x>=0 && x<256 && !(fillNative && edge))terrain=tile(layers[0],x,4);
      if(ceresUIShift && ceresCaptionAt(x))terrain=geometry.pixel(cx+x,cy+int(line));
      if(ceresUIShift && ceresCaptionAt(x-ceresUIShift)) {
        auto caption=tile(layers[0],x-ceresUIShift,4);if(caption.opaque)terrain=caption;
      }
    }
    bool suitPadding=!terrain.valid && eye && (eye->suit || eye->powerBomb || eye->xray || eye->motherBrain);
    if(suitPadding)main.color=sub.color=0;
    for(unsigned i=0;i<3;i++) {
      if(suitPadding && i<2)continue;
      auto fetched=layers[i];
      if constexpr(requires {geometry.draygonBodyMargin(x,line);})
        if(i==1 && !eye && fetched.base==0 && fetched.screen==0x4800 && fetched.size==1 &&
           geometry.draygonBodyMargin(x,line))fetched.above=1;
      if(!fetched.above && !fetched.below)continue; // disabled planes cannot contribute, including death backdrop
      if constexpr(requires {geometry.messageOverlay();}) {
        // UI text is native-screen content, not a repeating room background.
        // Keep its native pixels while revealing room layers in the margins.
        if(i==2 && (x<0 || x>=256) && geometry.messageOverlay() &&
           fetched.screen==0x5800 && fetched.size==0)continue;
      }
      if(i==2 && bg3Fetch && x<int(bg3Fetch->until)){fetched.screen=bg3Fetch->screen;fetched.size=bg3Fetch->size;}
      // A full-room BG2 sample often supersedes the native tile-ring sample.
      // Resolve that owner first, then decode only the selected tile.
      std::optional<Pixel> replacement;
      if constexpr(requires {geometry.background(x,int(line),layers[i].x,layers[i].y,layers[i].base*2,cx,cy);}) {
        if(i==1 && !layers[i].mosaic && !(ceresUIShift>=0 && x>=0 && x<256 && !(fillNative && edge)) && !((staged || edge) && x>=0 && x<256 && !fillNative)) {
          auto background=geometry.background(x,int(line),layers[i].x,layers[i].y,layers[i].base*2,cx,cy);
          if(background)replacement=*background;
        }
      }
      auto value=i==0?terrain:replacement?*replacement:tile(fetched,x,i==2?2:4);
      if(i==1 && eye && eye->xray && xrayTerrain && registers.get(Reg::window_bg2_twoEnable))value=xrayTerrain->pixel(cx+x,cy+int(line));
      if(!value.valid)return {};
      if(!value.opaque)continue;
      auto priority=value.priority?layers[i].high:layers[i].low;
      const Reg twoEnable[]={Reg::window_bg1_twoEnable,Reg::window_bg2_twoEnable,Reg::window_bg3_twoEnable};
      const Reg twoInvert[]={Reg::window_bg1_twoInvert,Reg::window_bg2_twoInvert,Reg::window_bg3_twoInvert};
      bool mask=eye && ((eye->powerBomb || eye->xray) ? registers.get(twoEnable[i]) && (eye->inside(x)!=bool(registers.get(twoInvert[i]))) : i==2 && registers.get(Reg::window_bg3_oneEnable) && (eye->inside(x)!=bool(registers.get(Reg::window_bg3_oneInvert))));
      if(fetched.above && !(mask && registers.get(Reg(unsigned(Reg::window_bg1_aboveEnable)+i*7))) && priority>main.priority)main={priority,value.color,i};
      if(fetched.below && !(mask && registers.get(Reg(unsigned(Reg::window_bg1_belowEnable)+i*7))) && priority>sub.priority)sub={priority,value.color,i};
    }
    auto get=[&](Reg key){return registers.get(key);};
    auto object=ship && !suitPadding?ship->pixel(x,line):std::nullopt;
    auto other=enemies && !suitPadding?enemies->pixel(x,line,fillNative):std::nullopt;
    if(ceresUIShift>=0 && enemies)other=enemies->nativePixel(x,line,ceresUIShift);
    if(other && (!object || other->order<object->order))object=other;
    if(object) {
      bool mask=eye && (eye->powerBomb || eye->xray) && get(Reg::window_obj_twoEnable) && (eye->inside(x)!=bool(get(Reg::window_obj_twoInvert)));
      if(get(Reg::obj_aboveEnable) && !(mask && get(Reg::window_obj_aboveEnable)) && object->priority>main.priority)main={object->priority,object->color,4};
      if(get(Reg::obj_belowEnable) && !(mask && get(Reg::window_obj_belowEnable)) && object->priority>sub.priority)sub={object->priority,object->color,4};
    }
    const Reg enables[]={Reg::screen_bg1_colorEnable,Reg::screen_bg2_colorEnable,
                         Reg::screen_bg3_colorEnable,Reg::screen_back_colorEnable,Reg::screen_obj_colorEnable};
    unsigned color=main.color;
    bool aboveColor=true,belowColor=true;
    if(eye && (eye->suit || eye->powerBomb || eye->xray || eye->motherBrain)) {
      bool inside=get((eye->powerBomb || eye->xray)?Reg::window_col_twoEnable:Reg::window_col_oneEnable) && (eye->inside(x)!=bool(get((eye->powerBomb || eye->xray)?Reg::window_col_twoInvert:Reg::window_col_oneInvert)));
      bool modes[]={true,inside,!inside,false};
      aboveColor=modes[get(Reg::window_col_aboveMask)];belowColor=modes[get(Reg::window_col_belowMask)];
      if(!aboveColor)color=0;
    }
    if(belowColor && get(enables[main.layer]) && (main.layer!=4 || object->palette>=4)) {
      bool useSub=get(Reg::screen_blendMode) && sub.priority;
      unsigned second=useSub?sub.color:(get(Reg::screen_colorRed)|(get(Reg::screen_colorGreen)<<5)|(get(Reg::screen_colorBlue)<<10));
      bool halve=get(Reg::screen_colorHalve) && aboveColor && (!get(Reg::screen_blendMode) || useSub);
      unsigned out=0;
      for(unsigned shift:{0,5,10}) {
        int a=(color>>shift)&31,b=(second>>shift)&31;
        int n=get(Reg::screen_colorMode)?std::max(0,a-b):a+b;
        if(halve)n/=2;
        out|=unsigned(std::min(31,n))<<shift;
      }
      color=out;
    }
    RGBPixel result{{},true};
    for(unsigned i=0;i<3;i++)result.rgb[i]=uint8_t((((color>>(i*5))&31)*get(Reg::displayBrightness)*16+15)/30);
    return result;
  }
};
}
