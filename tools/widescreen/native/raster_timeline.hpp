// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded visible-row replay. Initial images must come from the native raster.
#pragma once
#include "scene_selection.hpp"
#include "scene_geometry.hpp"
#include "mode1.hpp"
#include <string>
namespace SMPWide {
using MemoryRef=std::shared_ptr<const std::vector<uint8_t>>;
struct RetainedRoomGraphics {MemoryRef vram,palette;unsigned base,bg2Base,room,state;MemoryRef registers;};
using RetainedRoomGraphicsRef=std::shared_ptr<const RetainedRoomGraphics>;
inline MemoryRef memoryImage(Bytes data,size_t size) {
  if(data.size()!=size)throw std::invalid_argument("incomplete memory image");
  return std::make_shared<const std::vector<uint8_t>>(data.begin(),data.end());
}
inline Bytes memoryBytes(const MemoryRef& ref,size_t size) {
  if(!ref || ref->size()!=size)throw std::invalid_argument("missing memory image");
  return *ref;
}
inline Bytes ownerBytes(const OwnerRef& ref) {
  if(!ref)throw std::invalid_argument("missing scene owner");
  return ref->bytes;
}
inline int sceneCoordinate(unsigned scroll,unsigned origin,SceneKind kind) {
  unsigned value=(scroll-origin)&65535;
  // Ordinary camera shake can cross zero. Preserve a signed displacement;
  // door scenes retain their separately qualified wrapping/join convention.
  return kind==SceneKind::Ordinary && value>=32768?int(value)-65536:int(value);
}
inline SceneKind coordinateKind(const SceneSelection& scene) {
  if(scene.kind!=SceneKind::Staging)return scene.kind;
  auto view=ownerBytes(scene.view),assets=ownerBytes(scene.assets);
  unsigned game=word(view,0x998),phase=word(view,0x99c);
  // Mother Brain's outgoing fade retains the room's last earthquake offset.
  // It is still a stationary room: -1 is a one-pixel shake, not world65535.
  // Keep door-scroll wrapping once the destination header takes ownership.
  if(word(view,0x79b)==0xdd58 && word(view,0x7bb)==0xdd6e &&
     word(assets,0x79b)==0xdd58 && word(assets,0x7bb)==0xdd6e &&
     word(view,0x911)==0 && (game==9 || (game==11 &&
       (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7))))
    return SceneKind::Ordinary;
  return scene.kind;
}
// Sampler descriptors depend on immutable upload images and OBJ registers,
// not the scanline or background scroll. Retain their backing images with them.
struct SpriteContext {
  OwnerRef view,previous;
  MemoryRef vram,palette,registers,maps,rom;
  std::unique_ptr<ShipSampler> ship;
  std::unique_ptr<EnemySampler> enemies;
  std::unique_ptr<XrayTerrain> xray;
  SpriteContext(OwnerRef o,OwnerRef prev,MemoryRef v,MemoryRef p,MemoryRef r,MemoryRef m,MemoryRef e):
    view(std::move(o)),previous(std::move(prev)),vram(std::move(v)),palette(std::move(p)),registers(std::move(r)),maps(std::move(m)),rom(std::move(e)) {
    auto ram=ownerBytes(view);
    if(maps && word(ram,0x79b)==0x91f8 && gameplayPresentationState(word(ram,0x998)) && !Registers(*registers).get(Reg::displayDisable))
      ship=std::make_unique<ShipSampler>(ram,*maps,ram.subspan(0x370,544),*vram,*palette,*registers,
        previous && word(ownerBytes(previous),0x79b)==0x91f8?ownerBytes(previous):Bytes{});
    if(rom)enemies=std::make_unique<EnemySampler>(ram,*rom,*vram,*palette,*registers);
    if(rom && xrayOwned(ram))xray=std::make_unique<XrayTerrain>(ram,*rom,*vram,*palette,Registers(*registers).get(Reg::bg2_tiledataAddress)*2);
  }
};
class SpriteContextCache {
  std::shared_ptr<const SpriteContext> last;
public:
  std::shared_ptr<const SpriteContext> get(OwnerRef view,OwnerRef previous,MemoryRef v,MemoryRef p,MemoryRef r,MemoryRef maps,MemoryRef rom) {
    bool same=last && last->view==view && last->previous==previous && last->vram==v && last->palette==p && last->maps==maps && last->rom==rom;
    if(same) {
      Registers a(*last->registers),b(*r);
      for(auto k:{Reg::displayDisable,Reg::obj_baseSize,Reg::obj_interlace,Reg::obj_firstSprite,
          Reg::obj_tiledataAddress,Reg::obj_nameselect,Reg::obj_priority0,Reg::obj_priority1,Reg::obj_priority2,Reg::obj_priority3})
        if(a.get(k)!=b.get(k)){same=false;break;}
    }
    if(!same)last=std::make_shared<const SpriteContext>(view,previous,v,p,r,maps,rom);
    return last;
  }
};
struct RowContext {
  const SceneSelection scene;
  const MemoryRef vram,palette,registers,shipMaps;
  const OwnerRef shipPrevious;
  const RetainedRoomGraphicsRef departingGraphics;
  const int cx,cy;
  const SceneGeometry geometry;
  const std::shared_ptr<const SpriteContext> sprites;
  const Mode1<SceneGeometry> renderer;
  RowContext(SceneSelection s,MemoryRef v,MemoryRef c,MemoryRef r,unsigned line,std::optional<EyeRow> eye={},MemoryRef maps={},OwnerRef previous={},std::optional<Bg3Fetch> fetched={},MemoryRef enemyRom={},RetainedRoomGraphicsRef departing={},std::optional<bool> message={},SpriteContextCache* objectCache=nullptr):
    scene(std::move(s)),vram(std::move(v)),palette(std::move(c)),registers(std::move(r)),shipMaps(std::move(maps)),shipPrevious(std::move(previous)),departingGraphics(std::move(departing)),
    cx(sceneCoordinate(Registers(memoryBytes(registers,205)).get(Reg::hoffset),word(ownerBytes(scene.view),0x91d),coordinateKind(scene))),
    cy(sceneCoordinate(Registers(memoryBytes(registers,205)).get(Reg::voffset),word(ownerBytes(scene.view),0x91f),coordinateKind(scene))),
    geometry(ownerBytes(scene.assets),ownerBytes(scene.view),scene.previous?ownerBytes(scene.previous):Bytes{},
      memoryBytes(vram,65536),memoryBytes(palette,512),Registers(*registers).get(Reg::tiledataAddress)*2,
      bool(scene.previous),scene.exitX,scene.exitY,
      departingGraphics && scene.previous && departingGraphics->room==word(ownerBytes(scene.previous),0x79b) && departingGraphics->state==word(ownerBytes(scene.previous),0x7bb)?Bytes(*departingGraphics->vram):Bytes{},
      departingGraphics?Bytes(*departingGraphics->palette):Bytes{},departingGraphics?departingGraphics->base:0,departingGraphics?departingGraphics->bg2Base:0,departingGraphics && departingGraphics->registers?Bytes(*departingGraphics->registers):Bytes{},message,shipPrevious?ownerBytes(shipPrevious):Bytes{}),
    sprites(objectCache?objectCache->get(scene.view,shipPrevious,vram,palette,registers,shipMaps,enemyRom):
      std::make_shared<const SpriteContext>(scene.view,shipPrevious,vram,palette,registers,shipMaps,enemyRom)),
    renderer(geometry,*vram,*palette,*registers,line,cx,cy,std::move(eye),sprites->ship.get(),fetched,sprites->enemies.get(),sprites->xray.get()) {}
  RowContext(const RowContext&)=delete;
  RowContext& operator=(const RowContext&)=delete;
};
struct CommittedWrite {uint64_t cycle;unsigned space,address,width,value;};
inline bool inertCgadsubReset(unsigned line,unsigned h,const Registers& before,const Registers& after) {
  // The tube/Landing Site route reaches this no-op at h10. It is safe
  // throughout horizontal blank, before the first visible fetch at h58.
  if(line!=32 || h>=58 || before.get(Reg::bgMode)!=1 || after.get(Reg::bgMode)!=1 ||
     before.get(Reg::displayDisable) || after.get(Reg::displayDisable))return false;
  for(auto field:{Reg::screen_colorMode,Reg::screen_colorHalve,Reg::screen_bg1_colorEnable,
      Reg::screen_bg2_colorEnable,Reg::screen_bg3_colorEnable,Reg::screen_bg4_colorEnable,
      Reg::screen_obj_colorEnable,Reg::screen_back_colorEnable})
    if(before.get(field) || after.get(field))return false;
  return true;
}
// The game uses the PPU's signed multiplier for ordinary enemy arithmetic.
// M7A/M7B and their latch do not feed Mode 1 pixels. Keep these transactions
// ordered without creating a raster split or relaxing any visible-state check.
// OAM/CGRAM fetch addresses and the PPU1 read-data latch advance independently of
// software writes; neither changes the palette or any displayed pixel.
inline bool mode1MultiplyWrite(unsigned address,const Registers& before,const Registers& after) {
  if((address!=0x211b && address!=0x211c) || before.get(Reg::bgMode)!=1 || after.get(Reg::bgMode)!=1)return false;

  for(unsigned i=0;i<sizeof(registerFields)/sizeof(registerFields[0]);i++) {
    auto field=Reg(i);
    if(field==Reg::m7a || field==Reg::m7b || field==Reg::latch_mode7 || field==Reg::ppu1_mdr || field==Reg::latch_cgramAddress || field==Reg::latch_oamAddress)continue;
    if(before.get(field)!=after.get(field))return false;
  }
  return true;
}
class RowTimeline {
  SpriteContextCache* objectCache=nullptr;
  SceneSelection scene;
  MemoryRef vram,palette,shipMaps;
  MemoryRef enemyRom;
  RetainedRoomGraphicsRef departingGraphics;
  OwnerRef shipPrevious;
  unsigned line,lastH=0;
  uint64_t lastCycle=0;
  bool blank,sealed=false;
  std::optional<EyeRow> eye;
  std::optional<Bg3Fetch> bg3Fetch;
  std::optional<bool> liveMessage;
  std::vector<CommittedWrite> pending;
  std::optional<std::vector<uint8_t>> working;
  std::vector<std::pair<unsigned,std::unique_ptr<const RowContext>>> states;
  void mutableRow() const {if(sealed)throw std::logic_error("row already sealed");}
  void materialize(unsigned x,Bytes registers) {
    if(working) {
      vram=std::make_shared<const std::vector<uint8_t>>(std::move(*working));working.reset();
    }
    auto context=std::make_unique<const RowContext>(scene,vram,palette,memoryImage(registers,205),line,eye,shipMaps,shipPrevious,bg3Fetch,enemyRom,departingGraphics,liveMessage,objectCache);
    if(!states.empty() && states.back().first==x)states.back().second=std::move(context);
    else states.emplace_back(x,std::move(context));
  }
public:
  RowTimeline(SceneSelection s,MemoryRef v,MemoryRef c,Bytes registers,unsigned scanline,std::optional<EyeRow> effect={},MemoryRef maps={},OwnerRef previous={},MemoryRef enemies={},RetainedRoomGraphicsRef departing={},std::optional<bool> message={},SpriteContextCache* objectCache=nullptr):
    objectCache(objectCache),scene(std::move(s)),vram(std::move(v)),palette(std::move(c)),shipMaps(std::move(maps)),enemyRom(std::move(enemies)),departingGraphics(std::move(departing)),shipPrevious(std::move(previous)),line(scanline),
    blank(Registers(registers).get(Reg::displayDisable)),eye(std::move(effect)),liveMessage(message) {
    const auto game=word(ownerBytes(scene.view),0x998);
    if(!line || line>224 || (line<32 && !(game>=0x15 && game<=0x19)))
      throw std::invalid_argument("row outside terrain body");
    materialize(0,registers);
  }
  void memoryWrite(CommittedWrite w) {
    mutableRow();
    unsigned size=w.space==1?65536:w.space==2?544:w.space==3?512:0;
    if(!size || (w.width!=1 && w.width!=2) || w.address>size-w.width || w.value>=(1u<<(w.width*8)))
      throw std::invalid_argument("invalid committed memory write");
    pending.push_back(w);
  }
  void registerWrite(uint64_t cycle,unsigned h,unsigned address,Bytes after) {
    mutableRow();
    if(h>1078 || h<lastH || cycle<lastCycle)throw std::invalid_argument("invalid visible transaction order");
    unsigned space=address==0x2104?2:(address==0x2118 || address==0x2119)?1:address==0x2122?3:0;
    for(auto w:pending)if(w.cycle!=cycle || w.space!=space)
      throw std::invalid_argument("committed write lacks matching transaction");
    bool afterBlank=Registers(after).get(Reg::displayDisable);
    Registers before(*states.back().second->registers);
    bool earlyBg3=address==0x2109 && line==32 && h>0 && h<32 && !blank && !afterBlank &&
      before.get(Reg::bgMode)==1 && !before.get(Reg::bg3_tileSize) && !before.get(Reg::bg3_mosaicEnable);
    bool inertMultiply=mode1MultiplyWrite(address,before,Registers(after));
    bool inertReset=address==0x2131 && inertCgadsubReset(line,h,before,Registers(after));
    bool powerBombMathReset=address==0x2131 && line==32 && h<32 && eye && eye->powerBomb;
    bool inertBg3=address==0x2109 &&
      before.get(Reg::bg3_screenAddress)==Registers(after).get(Reg::bg3_screenAddress) &&
      before.get(Reg::bg3_screenSize)==Registers(after).get(Reg::bg3_screenSize);
    // Power Bomb + liquid HDMA can finish the line32 layer-enable write a
    // few pixels into the visible row. Replay it at its actual pixel boundary
    // instead of rejecting the frame and losing the effect's fetch history.
    bool layerEnable=address==0x212c || address==0x212d;
    // Startup HDMA delays this IRQ before it enables the visible PB window.
    // The committed native active flag owns that interval too; an EyeRow only
    // exists once the window is enabled. Preserve the actual split boundary.
    bool powerBombLayerEnable=layerEnable && line==32 &&
      ((eye && eye->powerBomb) || (word(ownerBytes(scene.view),0x592)&0x8000));
    // The room-entry colour-math reset can land at line32 h0. It precedes
    // every visible fetch, so apply its validated state at pixel0 instead of
    // dropping this frame to the original viewport.
    bool earlyColorControl=(address==0x2130 || address==0x2131) && line==32 && h<58;
    // The tube-removal handoff can delay Mother Brain's line32 IRQ until
    // h784. Replay its supported colour/layer reset at the real pixel split
    // so the arena reveal doesn't briefly drop to the native viewport.
    auto ram=ownerBytes(scene.view);
    bool motherArenaSetup=line==32 && word(ram,0x998)==8 && word(ram,0x7800)>=1 && word(ram,0x7800)<=2 &&
      smpMotherBrainRoom([&](unsigned a){return word(ram,a);});
    bool lateMotherColor=motherArenaSetup && (address==0x2130 || address==0x2131);
    bool control=address==0x2100 || earlyColorControl || lateMotherColor ||
      (layerEnable && (h<58 || powerBombLayerEnable || motherArenaSetup));
    if(address>=0x2115 && address<=0x2119) {
      if(!blank || !afterBlank)throw std::invalid_argument("VRAM transfer requires forced blank");
    } else if(control || earlyBg3 || inertReset || inertBg3 || powerBombMathReset || inertMultiply) {
      if(!pending.empty())throw std::invalid_argument("unexpected control memory effect");
      if(earlyBg3 && !bg3Fetch)bg3Fetch=Bg3Fetch{8-(before.get(Reg::bg3_hoffset)&7),before.get(Reg::bg3_screenAddress),before.get(Reg::bg3_screenSize)};
    } else {
      Registers before(*states.back().second->registers),next(after);
      throw std::invalid_argument("unsupported visible register write address="+std::to_string(address)+" line="+std::to_string(line)+" h="+std::to_string(h)+
        " bg3map="+std::to_string(before.get(Reg::bg3_screenAddress))+"/"+std::to_string(before.get(Reg::bg3_screenSize))+
        "->"+std::to_string(next.get(Reg::bg3_screenAddress))+"/"+std::to_string(next.get(Reg::bg3_screenSize)));
    }
    for(auto w:pending) {
      if(!working)working.emplace(*vram);
      for(unsigned i=0;i<w.width;i++)(*working)[w.address+i]=uint8_t(w.value>>(i*8));
    }
    pending.clear();
    if(control || earlyBg3 || inertReset || inertBg3 || powerBombMathReset)materialize(h<58?0:(h-58+3)/4,after);
    blank=afterBlank;lastH=h;lastCycle=cycle;
  }
  void finish() {
    mutableRow();
    if(!pending.empty())throw std::invalid_argument("unpaired final memory write");
    sealed=true;
  }
  const RowContext& at(int x) const {
    if(!sealed)throw std::logic_error("row is not sealed");
    unsigned edge=unsigned(std::clamp(x,0,255));
    for(auto it=states.rbegin();it!=states.rend();++it)if(it->first<=edge)return *it->second;
    throw std::logic_error("missing initial row state");
  }
  bool centerCandidate(int x) const {auto& r=at(x);if(r.renderer.stagedCrocomire())return r.renderer.stagingCandidate(x);auto* e=r.sprites->enemies.get();if(r.renderer.terrainEdgeCandidate(x))return !e || e->recomposeArenaWall(x,line) || !e->nativeObjectAt(x,line);return e && e->hasCenterFill() && e->pixel(x,line,true) && !e->nativeObjectAt(x,line);}
  std::pair<int,int> centerRange() const {
    int first=256,last=0;
    for(const auto& s:states) {
      const auto& row=*s.second;
      // Relocated actors and Crocomire can occupy the whole native field.
      if(row.renderer.stagedCrocomire() || (row.sprites->enemies && row.sprites->enemies->hasCenterFill()))return {0,256};
      // Terrain repairs are contiguous strips at either native edge. Scan
      // those short strips once, rather than testing all256columns per row.
      // Union every raster state; centerCandidate still chooses the actual
      // state for each pixel and retains its native-background witness.
      int left=0;while(left<256 && row.renderer.terrainEdgeCandidate(left))++left;
      int right=256;while(right>0 && row.renderer.terrainEdgeCandidate(right-1))--right;
      if(left){first=0;last=std::max(last,left);}
      if(right<256){first=std::min(first,right);last=256;}
    }
    return {first,last};
  }
  bool hasCenterFill() const {auto range=centerRange();return range.first<range.second;}
  bool ceresUICandidate(int x) const {return at(x).renderer.ceresUICandidate(x);}
  RGBPixel pixel(int x,bool fillNative=false,int ceresUIShift=-1) const {return at(x).renderer.pixel(x,fillNative,ceresUIShift);}
};
}
