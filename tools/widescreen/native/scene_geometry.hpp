// SPDX-License-Identifier: GPL-3.0-or-later
// Immutable selected-scene geometry; readiness/ownership is supplied by the caller.
#pragma once
#include "terrain.hpp"
#include "registers.hpp"
#include "crocomire_staging.hpp"
#include "../host/mother_brain_arena.hpp"
#include "../host/ceres_arena.hpp"
#include <optional>
#include <algorithm>
namespace SMPWide {
class SceneGeometry {
  Bytes assetsRam,viewRam,priorRam,vramImage,paletteImage;
  Bytes previousCameraRam;
  Bytes departingVram,departingPalette,departingRegisters;
  unsigned departingBase=0,departingBg2Base=0;
  bool kraidEntry=false,departingFinished=false;
  bool nativeCeresArena=false,nativeMotherArena=false;
  enum class QuakeEdge {None,Escape,Left,Right};
  QuakeEdge quakeEdge=QuakeEdge::None;
  int quakeCamera=0;
  std::optional<bool> liveMessage;
  Terrain current;
  std::optional<Terrain> previous;
  int direction=-1,seam=0,oldX=0,oldY=0,destinationY=0,dx=0,dy=0;
  struct Location {const Terrain* terrain;int x,y;};
  Location location(int x,int y) const {
    if(direction<0)return {&current,x,y};
    if(direction==0 || direction==1) {
      if(direction==0 && x>=32768)x-=65536;
      if(direction==1 ? x<seam : x>=seam)return {&current,x,y};
      if(departingFinished)return {nullptr,0,0};
      return {&*previous,oldX+x-seam+(direction==0?256:0),oldY+y-destinationY};
    }
    if(y>=32768)y-=65536;
    if(direction==2 ? y>=seam : y<seam)return {&current,x,y};
    return {&*previous,x+dx,y+dy};
  }
  void qualifyQuakeEdge() {
    quakeCamera=int(word(viewRam,0x911));
    if(word(viewRam,0x998)!=8)return;
    // Escape quake6 exposes up to three stale native BG1 ring columns. They
    // become conspicuous black strips during the Landing Site's bright flash.
    // Repair only those columns, with the usual native-background witness and
    // OBJ exclusion; ordinary room edges and all other pixels stay untouched.
    bool landingEscape=word(viewRam,0x79b)==0x91f8 && word(viewRam,0x7bb)==0x9261 &&
      word(assetsRam,0x79b)==0x91f8 && word(viewRam,0x183e)==6;
    // Parlor's escape uses quake24 with the same three-pixel displacement.
    // Its moving camera also exposes stale native ring columns at either edge.
    bool parlorEscape=word(viewRam,0x79b)==0x92fd && word(viewRam,0x7bb)==0x9348 &&
      word(assetsRam,0x79b)==0x92fd && word(assetsRam,0x7bb)==0x9348 && word(viewRam,0x183e)==24;
    unsigned room=word(viewRam,0x79b),state=word(viewRam,0x7bb);
    bool ceresEscape=word(viewRam,0x79f)==6 && word(viewRam,0x183e)==20 &&
      word(assetsRam,0x79b)==room && word(assetsRam,0x7bb)==state &&
      ((room==0xe06b && state==0xe097) || (room==0xe021 && state==0xe04d) ||
       (room==0xdfd7 && state==0xe003) || (room==0xdf8d && state==0xdfb9));
    // Ceres decrements its one-frame quake timer before the shaken registers
    // are displayed. The live raster displacement remains the witness even
    // when that counter has already reached zero.
    if(!word(viewRam,0x1840) && !ceresEscape)return;
    if((landingEscape || parlorEscape || ceresEscape) && word(viewRam,0x943))
      {quakeEdge=QuakeEdge::Escape;return;}
    if(word(viewRam,0x79b)==0xa923 && word(viewRam,0x7bb)==0xa930 && word(viewRam,0x183e)==0x14)
      {quakeEdge=QuakeEdge::Left;return;}
    // Amphitheatre lavaquake moves the displayed camera two pixels right.
    // At the room's right limit those last native columns wrap stale VRAM.
    if(word(viewRam,0x79b)==0xb4e5 && word(viewRam,0x7bb)==0xb4f2 &&
      word(assetsRam,0x79b)==0xb4e5 && word(viewRam,0x183e)==0x15 &&
      word(viewRam,0x911)+256==word(assetsRam,0x7a5)*16)quakeEdge=QuakeEdge::Right;
  }
public:
  bool motherBrainArena() const {
    return word(viewRam,0x911)==0 && ((direction<0 && nativeMotherArena) || ceresArena());
  }
  bool ceresArena() const {
    return (direction<0 || (direction==0 && departingFinished)) && word(viewRam,0x911)==0 &&
      nativeCeresArena;
  }
  bool wideBossEdgeCandidate(int x,int camera=0,unsigned backgroundScroll=0) const {
    if(!motherBrainArena())return false;
    // Keep the Ceres emergency caption's native tiles. Only its old wall
    // strip needs background replacement; Mother Brain also moves machinery.
    // Emergency quake moves BG2 independently of BG1. Follow both raster
    // offsets so the native background wall's leading column is repaired too.
    int background=int((backgroundScroll-word(viewRam,0x921))&65535);
    if(background>=32768)background-=65536;
    int edge=word(viewRam,0x79b)==0xe0b5?240-std::clamp(std::max(camera,background),0,4):176;
    return x>=edge && x<256;
  }
  bool draygonBodyMargin(int x,unsigned line) const {
    // 88:DF94 disables the body BG2 outside the original horizontal bounds.
    // Its live tilemap/scroll still exists. Reveal it only in the extra margin,
    // retaining the native invisible flag and vertical HDMA bands.
    if((x>=0 && x<256) || x< -43 || x>=299 || direction>=0 ||
       !gameplayPresentationState(word(viewRam,0x998)) || word(viewRam,0x79b)!=0xda60 ||
       word(assetsRam,0x79b)!=0xda60 || word(viewRam,0xf78)!=0xde3f || (word(viewRam,0xf86)&0x200))return false;
    auto signed16=[](unsigned n){return n&0x8000?int(n&65535)-65536:int(n&65535);};
    int dx=signed16(word(viewRam,0xf7a)-word(viewRam,0x911));
    int dy=signed16(word(viewRam,0xf7e)-word(viewRam,0x915));
    if(!((dx>=-107 && dx< -64) || (dx>=320 && dx<363)) || dy< -16 || dy>=304 || line<32 || line>224)return false;
    return dy<40?line<96:dy>=192?line>=128:true;
  }
  bool unusedPadding(int x,int y) const {
    unsigned room=word(assetsRam,0x79b),state=word(assetsRam,0x7bb);
    unsigned game=word(viewRam,0x998),phase=word(viewRam,0x99c);
    bool departing=game==9 || (game==11 && (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7));
    bool ceres=(room==0xdf8d && (state==0xdf9f || state==0xdfb9)) ||
      (room==0xdfd7 && (state==0xdfe9 || state==0xe003)) ||
      (room==0xe021 && (state==0xe033 || state==0xe04d)) ||
      (room==0xe06b && (state==0xe07d || state==0xe097));
    // Ceres's repeating library background is not scenery beyond its physical
    // room walls. Stop that ring at the authored edge, including departure fade.
    if((direction<0 || ceresArena()) && ceres && word(viewRam,0x79b)==room && word(viewRam,0x7bb)==state &&
       (gameplayPresentationState(game) || departing))return x<0 || x>=int(word(assetsRam,0x7a5)*16);
    if(!gameplayPresentationState(game) && game!=9 && game!=11 && !ceresArena())return false;
    if((gameplayPresentationState(game) || departing || ceresArena()) &&
       (direction<0 || ceresArena()) && room==0xe0b5 && (state==0xe0c7 || state==0xe0e1) && word(viewRam,0x79b)==room && word(viewRam,0x7bb)==state) {
      int edge=nativeCeresArena?336:256;
      if(x<0 || x>=edge)return true;
    }
    // Keep filler hidden while the selected scene owns the door transition.
    // During scrolling, resolve the cell against the same departing/current
    // room as terrain; the live header can change before the displayed room.
    auto selected=location(x,y);
    if(!selected.terrain)return false;
    Bytes cells=selected.terrain==&current?assetsRam:priorRam;
    room=word(cells,0x79b);state=word(cells,0x7bb);
    x=selected.x;y=selected.y;
    if(!((room==0xd0b9 && state==0xd0c6) || (room==0xcfc9 && state==0xcfd6) || (room==0xd340 && state==0xd34d) || (room==0xb6ee && state==0xb6fb)))return false;
    unsigned width=word(cells,0x7a5),height=word(cells,0x7a7);
    if(x<0 || y<0 || unsigned(x)>=width*16 || unsigned(y)>=height*16)return false;
    // Retail solid filler metatile3ff is unused padding, not room decoration.
    // Its tile can show a diagnostic cross or graphics left by a prior room.
    // These cells lie beyond the native camera limits.
    return (word(cells,0x10002+(unsigned(y/16)*width+unsigned(x/16))*2)&0xf3ff)==0x83ff;
  }
  bool stagedCrocomire() const {return direction<0 && crocomireStaging(viewRam) && word(assetsRam,0x79b)==0xa98d;}
  std::optional<Pixel> crocomireWall(int x,int y) const {
    if(!stagedCrocomire() || x<469 || x>=560 || y<48 || y>=176 || word(assetsRam,0x7a5)!=128)return {};
    unsigned row=unsigned(y)/16;
    unsigned rock=word(assetsRam,0x10002+(row*128+32)*2);
    if((rock!=0x8140 && rock!=0x8120) ||
       word(assetsRam,0x10002+(row*128+34)*2)!=0xa49c)return {};
    // Same 48px wall, translated 43px left. Vacated cells use the native
    // repeating room backdrop, not a stretched rock slab or a black mask.
    return x<517?current.pixel(x+43,y):current.pixel(x+64,y);
  }
  bool quakeEdgeCandidate(int x,int camera) const {
    const int shift=quakeCamera-camera;
    switch(quakeEdge) {
    case QuakeEdge::Escape:return (shift>0 && shift<=3 && x>=0 && x<shift) ||
      (shift<0 && shift>=-3 && x>=256+shift && x<256);
    case QuakeEdge::Left:return x>=0 && x<shift && shift<=4;
    case QuakeEdge::Right:return shift==-2 && x>=254 && x<256;
    default:return false;
    }
  }
  bool ceresFloorOutside(int x,unsigned scroll) const {
    if(!ceresArena())return false;
    int position=int16_t(word(viewRam,0x917));
    int shift=int((scroll-word(viewRam,0x921)-unsigned(position)+256)&511)-256;
    // The floor IRQ displays BG2 alone. Its independent quake can reach the
    // padded cells past the relocated wall while BG1's camera remains still.
    int source=position+shift+x;return source<0 || source>=336;
  }
  bool quakeOutsideRoom(int x,int camera) const {
    if(word(viewRam,0x79f)==6 && quakeEdgeCandidate(x,camera))
      return camera+x<0 || camera+x>=int(word(assetsRam,0x7a5)*16);
    return word(viewRam,0x79b)==0xb4e5 && quakeEdgeCandidate(x,camera) &&
      camera+x>=int(word(assetsRam,0x7a5)*16);
  }
  // Retail message boxes temporarily select a32x32 BG3 map via $5B=$58.
  // The message index persists afterwards, so it alone is not an active flag.
  bool messageOverlay() const {return liveMessage.value_or(viewRam[0x5b]==0x58 && word(viewRam,0x1c1f)!=0);}
  SceneGeometry(Bytes assets,Bytes view,Bytes prior,Bytes vram,Bytes palette,unsigned base,
                bool joined,int exitX,int exitY,Bytes oldVram={},Bytes oldPalette={},unsigned oldBase=0,unsigned oldBg2Base=0,Bytes oldRegisters={},std::optional<bool> message={},Bytes previousCamera={}):assetsRam(assets),viewRam(view),priorRam(prior),vramImage(vram),paletteImage(palette),previousCameraRam(previousCamera),departingVram(oldVram),departingPalette(oldPalette),departingRegisters(oldRegisters),departingBase(oldBase),departingBg2Base(oldBg2Base),current(assets,vram,palette,base),oldX(exitX),oldY(exitY) {
    liveMessage=message;
    if(view.size()!=131072)throw std::invalid_argument("incomplete view snapshot");
    // These guards depend only on this immutable raster snapshot. Cache them
    // once rather than re-reading room topology for every composed pixel.
    nativeCeresArena=smpCeresArena([&](unsigned a){return word(view,a);});
    nativeMotherArena=smpMotherBrainArena([&](unsigned a){return word(view,a);});
    // Quake qualification is constant for this immutable owner snapshot.
    // Only the displayed camera displacement varies among raster contexts.
    qualifyQuakeEdge();
    if(!joined) {
      if(!prior.empty())throw std::invalid_argument("unexpected prior scene");
      return;
    }
    auto adjacent=[](unsigned room){return room==0xa56b || room==0xa6e2;};
    kraidEntry=(word(view,0x791)&3)<2 && ((word(assets,0x79b)==0xa59f && adjacent(word(prior,0x79b))) || (word(prior,0x79b)==0xa59f && adjacent(word(assets,0x79b))));
    if(kraidEntry && (oldVram.size()!=65536 || oldPalette.size()!=512))throw std::invalid_argument("Kraid entry lacks retained departing graphics");
    previous.emplace(prior,kraidEntry?oldVram:vram,kraidEntry?oldPalette:palette,kraidEntry?oldBase:base);
    if(word(view,0x998)!=11 || word(view,0x78d)!=word(prior,0x78d))
      throw std::invalid_argument("unmatched transition context");
    if(word(assets,0x79b)!=word(view,0x79b) || word(assets,0x7bb)!=word(view,0x7bb))
      throw std::invalid_argument("mixed current scene");
    direction=word(view,0x791)&3;
    if(oldX%256 || (direction<2 && oldY%256))throw std::invalid_argument("unaligned exit");
    destinationY=word(view,0x929);
    seam=direction<2?word(view,0x927)+(direction==1?256:0):destinationY+(direction==3?224:0);
    int camera=word(view,0x911);if(camera>=32768)camera-=65536;
    // Once the native viewport has reached the destination, the departing
    // room is no longer visible. Its tiles may already have been replaced in
    // VRAM; do not expose them again in the added horizontal margin during
    // the destination fade-in. This applies to ordinary doors as well as Kraid.
    departingFinished=direction<2 && (direction==0?camera>=seam:camera+256<=seam);
    auto difference=[&](unsigned address) {
      unsigned value=(unsigned(word(view,address))-unsigned(word(prior,address)))&65535;
      return value>=32768?int(value)-65536:int(value);
    };
    dx=difference(0x91d);dy=difference(0x91f);
  }
  Pixel pixel(int x,int y) const {
    // The Climb's unviewed side cells use solid filler3FF with a blank tile.
    // Continue its existing brick metatile in those added margin cells only;
    // retain every authored wall detail, door and the native viewport.
    unsigned game=word(viewRam,0x998),phase=word(viewRam,0x99c);
    bool departing=game==9 || (game==11 && (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7));
    if(direction<0 && (gameplayPresentationState(game) || departing) &&
       word(assetsRam,0x79b)==0x96ba && word(assetsRam,0x7bb)==0x96eb &&
       word(viewRam,0x79b)==0x96ba && word(viewRam,0x7bb)==0x96eb &&
       word(viewRam,0x911)==256 && word(assetsRam,0x7a5)==48 && word(assetsRam,0x7a7)==144 &&
       ((x>=213 && x<256) || (x>=512 && x<555)) && y>=0 && y<2304 &&
       word(assetsRam,0x10002+(unsigned(y/16)*48+unsigned(x/16))*2)==0x83ff) {
      unsigned q=(unsigned(y)%16/8)*2+unsigned(x)%16/8;
      return current.pixelEntry(word(assetsRam,0xa000+0x2e1*8+q*2),x,y);
    }
    auto p=location(x,y);return p.terrain?p.terrain->pixel(p.x,p.y):Pixel{};
  }
  std::optional<Pixel> background(int x,int line,unsigned scrollX,unsigned scrollY,unsigned base,int displayX,int displayY) const {
    if(kraidEntry) {
      int worldX=displayX+x;if(worldX>=32768)worldX-=65536;
      if(direction==0?worldX<seam:worldX>=seam) {
        if(departingFinished)return Pixel{};
        Terrain bg(priorRam,departingVram,departingPalette,departingBg2Base,1);
        int sx=worldX-seam+(direction==0?256:0),sy=displayY+line-destinationY;
        if(word(priorRam,0x79b)!=0xa59f)return bg.pixel(oldX+sx,oldY+sy);
        if(departingRegisters.size()!=205)throw std::invalid_argument("Kraid departure lacks background registers");
        Registers saved(departingRegisters);unsigned px=unsigned(sx+int(saved.get(Reg::bg2_hoffset))),py=unsigned(sy+int(saved.get(Reg::bg2_voffset)));
        unsigned size=saved.get(Reg::bg2_screenSize),tx=(px/8)&(size&1?63:31),ty=(py/8)&(size&2?63:31);
        unsigned screen=tx/32+(ty/32)*(size&1?2:1);
        auto entry=word(departingVram,((saved.get(Reg::bg2_screenAddress)+screen*1024+(ty%32)*32+tx%32)&32767)*2);
        return bg.pixelEntry(entry,int(px),int(py));
      } else if(word(assetsRam,0x79b)!=0xa59f) {
        int bx=int((scrollX-word(viewRam,0x921))&65535),by=int((scrollY-word(viewRam,0x923))&65535);
        if(bx>=32768)bx-=65536;if(by>=32768)by-=65536;
        Terrain bg(assetsRam,vramImage,paletteImage,base,1);return bg.pixel(bx+x,by+line);
      }
    }
    // Crocomire owns BG2 as a single boss tilemap, not a repeating backdrop.
    // Retail A4:8BA4 parks it at scroll100h when the body is outside the native
    // view. Extending that parked map would wrap the boss into the right margin.
    unsigned game=word(viewRam,0x998),phase=word(viewRam,0x99c);
    // Mother Brain's second-phase body is one 32x32 BG2 image, not a repeating
    // room backdrop. Native scroll wraps within that image; the extra margins
    // must not show a second copy after crossing its horizontal extent.
    // The completed escape caption shares this single BG2 map. Keep its
    // non-repeating extent during the outgoing door fade, before room loading
    // clears the typewriter; otherwise its first letters wrap into the margin.
    bool escapeCaptionFade=(game==9 || (game==11 &&
      (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7))) &&
      word(viewRam,0x8036)==0xc4c9 && word(viewRam,0x943);
    if((gameplayPresentationState(game) || escapeCaptionFade) && word(assetsRam,0x79b)==0xdd58 &&
       word(assetsRam,0x7bb)==0xdd6e && word(viewRam,0x79b)==0xdd58 &&
       word(viewRam,0xf78)==0xec7f &&
       ((word(viewRam,0x7800)>=2 && word(viewRam,0x7800)<=4) ||
        (word(viewRam,0x7800)==1 && word(viewRam,0x178c)==0x87c9 && viewRam[0x178e]==0xa9)) &&
       !(viewRam[0x59]&3) && word(viewRam,0x911)==0) {
      int origin=34-int(word(viewRam,0xf7a));
      int delta=int((scrollX-unsigned(origin)+128)&255)-128;
      int px=x+origin+delta;
      if(px<0 || px>=256)return Pixel{0,0,false,true};
      unsigned py=(unsigned(line)+scrollY)&255,map=(unsigned(viewRam[0x59])&0xfc)<<8;
      auto entry=word(vramImage,((map+(py/8)*32+unsigned(px)/8)&32767)*2);
      Terrain body(assetsRam,vramImage,paletteImage,base,1);
      return body.pixelEntry(entry,px,int(py));
    }
    bool crocomireEntry=game==11 && (phase==0xe664 || phase==0xe6a2 || phase==0xe737);
    if(word(assetsRam,0x79b)==0xa98d && word(viewRam,0x79b)==0xa98d && word(viewRam,0xf78)==0xddbf &&
        (gameplayPresentationState(game) || crocomireEntry) && base==0 && viewRam[0x59]==0x49) {
      if(word(viewRam,0xf86)&0x300)return Pixel{0,0,false,true};
      // The native body map is cleared before the skeleton flows back (58h).
      // Subsequent phases use OBJ only. The second BG2 page is not uploaded:
      // sampling its zero entries exposes unrelated tile0 pixels in margins.
      unsigned death=word(viewRam,0xfa8);
      if(death==0x58 || (death>=0x3e && death<=0x56 && !(death&1)))
        return Pixel{0,0,false,true};
      int sx=int(uint16_t(word(viewRam,0x911)+0x33-word(viewRam,0xf7a)));
      if(sx>=32768)sx-=65536;
      // A4:8BA4 computes BG2 before the camera advances. Parked-map recovery
      // must use that same camera phase, or the handoff to native BG2 changes
      // the body's position for one frame when it first approaches the edge.
      if(death==0 && (scrollX&511)==256 && previousCameraRam.size()==131072 &&
          word(previousCameraRam,0x79b)==0xa98d && word(previousCameraRam,0xf78)==0xddbf &&
          gameplayPresentationState(word(previousCameraRam,0x998))) {
        sx=int(uint16_t(word(previousCameraRam,0x911)+0x33-word(viewRam,0xf7a)));
        if(sx>=32768)sx-=65536;
      }
      if((scrollX&511)==(unsigned(sx)&511) || (scrollX&511)==256) {
        int px=x+sx;unsigned py=(unsigned(line)+scrollY)&255;
        if(px<0 || px>=512)return Pixel{0,0,false,true};
        unsigned tx=unsigned(px)/8,ty=py/8;
        auto entry=word(vramImage,(0x4800+(tx/32)*1024+ty*32+tx%32)*2);
        Terrain bg(assetsRam,vramImage,paletteImage,base,1);
        return bg.pixelEntry(entry,px,int(py));
      }
    }
    // Warehouse Kihunter has a room-defined BG2 (8F:A4E7, libraryBG0,
    // layer2Scrolls C0/00). The native VRAM ring is populated as the camera
    // moves; it is not valid evidence for newly exposed widescreen margins.
    unsigned room=word(assetsRam,0x79b),state=word(assetsRam,0x7bb);
    bool roomBackground=(room==0xa4da && state==0xa4e7) || (room==0xa56b && state==0xa578) || (room==0xa6e2 && state==0xa6ef) ||
      (room==0xa7b3 && state==0xa7c0) || (room==0xaf72 && state==0xaf7f) ||
      (room==0xadad && state==0xadba) || (room==0xa788 && state==0xa795) || (room==0xa322 && state==0xa32f) ||
      (room==0xa293 && state==0xa2a0) || (room==0xcf80 && state==0xcf8d) ||
      (room==0xaa82 && state==0xaa8f) || (room==0xab8f && state==0xab9c) ||
      (room==0xd104 && state==0xd111) || (room==0xd95e && (state==0xd970 || state==0xd98a)) ||
      (room==0xcc27 && (state==0xcc39 || state==0xcc53)) ||
      (room==0xcfc9 && state==0xcfd6) || (room==0xc98e && state==0xc9ba) || (room==0xd340 && state==0xd34d) || (room==0xd2d9 && state==0xd2e6) || (room==0xd017 && state==0xd024) || (room==0xd0b9 && state==0xd0c6) || (room==0xd16d && state==0xd17a) || (room==0xd13b && state==0xd148) ||
      (room==0xd72a && state==0xd737) || (room==0xd78f && state==0xd7a1) ||
      (room==0xd1dd && state==0xd1ea) || (room==0xd1a3 && state==0xd1b0) ||
      (room==0xd5a7 && state==0xd5b4) || (room==0xd6fd && state==0xd70a) || (room==0xd617 && state==0xd624) ||
      (room==0xb6ee && state==0xb6fb) || (room==0xe021 && (state==0xe033 || state==0xe04d)) ||
      (room==0xe0b5 && (state==0xe0c7 || state==0xe0e1)) || (room==0x9c5e && state==0x9c6b) ||
      (room==0x9cb3 && state==0x9cc0) || (room==0xaa41 && state==0xaa4e);
    // These reported routes have already scrolled into the destination,
    // but remain in the door fade for several frames. Use destination BG2
    // there too; its offscreen native ring still belongs to earlier columns.

    bool qualifiedArrival=game==11 && direction==1 && departingFinished &&
      ((room==0xcfc9 && state==0xcfd6 && word(priorRam,0x79b)==0xd08a) ||
       (room==0xd340 && state==0xd34d && word(priorRam,0x79b)==0xd433) ||
       (room==0xd2d9 && state==0xd2e6 && word(priorRam,0x79b)==0xd30b)) &&
      (phase==0xe664 || phase==0xe6a2 || phase==0xe737);
    bool upperMainStreetArrival=game==11 && direction==3 &&
      room==0xcfc9 && state==0xcfd6 && word(priorRam,0x79b)==0xcefb &&
      word(priorRam,0x7bb)==0xcf27 && word(viewRam,0x915)==unsigned(destinationY) &&
      (phase==0xe664 || phase==0xe6a2 || phase==0xe737);
    // Cathedral and the upper Main Street/Mt. Everest exits still need their
    // full-room BG2 during departure. The native ring lacks those columns. Stop before
    // the destination header/scroll takes ownership of the background.
    bool roomDeparture=direction<0 && ((room==0xa788 && state==0xa795) ||
      (room==0xcfc9 && state==0xcfd6) || (room==0xd0b9 && state==0xd0c6)) &&
      word(viewRam,0x79b)==room && word(viewRam,0x7bb)==state &&
      (game==9 || (game==11 && (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7)));
    bool ceresBackground=((room==0xe021 && (state==0xe033 || state==0xe04d)) ||
      (room==0xe0b5 && (state==0xe0c7 || state==0xe0e1))) &&
      word(viewRam,0x79b)==room && word(viewRam,0x7bb)==state &&
      (game==9 || (game==11 && (direction<0 || departingFinished)));
    // These room-defined backgrounds remain necessary during door fades;
    // their offscreen VRAM columns can still belong to the previous room.
    bool roomFadeBackground=((room==0x9c5e && state==0x9c6b) || (room==0x9cb3 && state==0x9cc0) ||
      (room==0xaa41 && state==0xaa4e) || (room==0xaa82 && state==0xaa8f)) &&
      word(viewRam,0x79b)==room && word(viewRam,0x7bb)==state &&
      (game==9 || (game==11 && (direction<0 || departingFinished)));
    if((gameplayPresentationState(game) || qualifiedArrival || upperMainStreetArrival || roomDeparture || ceresBackground || roomFadeBackground) && roomBackground) {
      auto signedOffset=[](unsigned value){value&=65535;return value>=32768?int(value)-65536:int(value);};
      // HDMA writes scroll values wrapped to the native tile ring. Resolve
      // that phase around the full layer2 position, including shake/heat offsets.
      auto coordinate=[&](unsigned scroll,unsigned offset,unsigned position,unsigned period) {
        int delta=int((scroll-offset-position+period/2)&(period-1))-int(period/2);
        return signedOffset(position)+delta;
      };
      int bx=coordinate(scrollX,word(viewRam,0x921),word(viewRam,0x917),512);
      int by=coordinate(scrollY,word(viewRam,0x923),word(viewRam,0x919),256);
      Terrain background(assetsRam,vramImage,paletteImage,base,1);
      return background.pixel(bx+x,by+line);
    }
    if(direction==1 && word(priorRam,0x79b)==0x97b5 && word(priorRam,0x7bb)==0x97e0) {
      if(word(assetsRam,0x79b)!=0x975c || word(assetsRam,0x7bb)!=0x9787 || oldX || oldY)
        throw std::invalid_argument("unqualified departing custom background");
      if(displayX+x>=seam) {
        Terrain oldBackground(priorRam,vramImage,paletteImage,base,1);
        return oldBackground.pixel(oldX+displayX+x-seam,oldY+displayY+line-destinationY);
      }
    }
    const bool ordinary=gameplayPresentationState(word(viewRam,0x998));
    const bool joined=direction==0 && word(viewRam,0x998)==11;
    if(word(assetsRam,0x79b)!=0x97b5 || word(assetsRam,0x7bb)!=0x97c6 || (!ordinary && !joined))return {};
    int cameraX=(scrollX-word(viewRam,0x921))&65535;
    int cameraY=(scrollY-word(viewRam,0x923))&65535;
    if(joined && cameraX>=32768)cameraX-=65536;
    Terrain bg(assetsRam,vramImage,paletteImage,base,1);
    if(!joined || cameraX+x>=0)return bg.pixel(cameraX+x,cameraY+line);
    if(word(priorRam,0x79b)!=0x975c || word(priorRam,0x7bb)!=0x976d)
      throw std::invalid_argument("unqualified prior repeating background");
    unsigned px=unsigned(x+int(scrollX)),py=unsigned(line+int(scrollY));
    auto tile=word(priorRam,0x4000+(((py/8)&31)*32+((px/8)&31))*2);
    return bg.pixelEntry(tile,int(px),int(py));
  }
  bool entry(int x,int y,uint16_t& value) const {auto p=location(x,y);return p.terrain && p.terrain->entry(p.x,p.y,value);}
};
}

