// SPDX-License-Identifier: GPL-3.0-or-later
// Native stream coupling. The host supplies qualified scheduler-frame boundaries.
#pragma once
#include "raster_timeline.hpp"
#include "row_executor.hpp"
#include "mother_pillar_reveal.hpp"
namespace SMPWide {
struct WideFrame {std::vector<uint8_t> rgb;uint64_t clipped=0;std::vector<uint8_t> centerMask;bool blur=false;};
class FramePipeline {
  struct Raster {SceneSelection scene;MemoryRef vram,palette,registers;std::optional<EyeRow> eye;OwnerRef shipPrevious;RetainedRoomGraphicsRef departing;std::optional<bool> message;};
  RetainedRoomGraphicsRef lastOrdinaryGraphics,transitionGraphics;
  struct Change {uint64_t cycle;unsigned h,address;MemoryRef registers;std::vector<CommittedWrite> writes;};
  SceneSelector selector;
  EyeState eye;
  bool eyeEnabled=false;
  std::optional<SceneSelection> captured,uploaded;
  MemoryRef shipMaps;
  MemoryRef enemyRom;
  OwnerRef capturedPrevious,uploadedPrevious;
  std::array<std::optional<Raster>,225> rasters;
  std::array<std::vector<Change>,225> changes;
  std::vector<CommittedWrite> pending;
  bool open=false,sealed=false;
  bool motherSceneSeen=false,motherWasExpanded=false;
  unsigned motherPanOffset=0;
  MotherPillarReveal motherPillar;
  const char* deferredDisplayError=nullptr;
  void writable() const {if(!open || sealed)throw std::logic_error("frame is not writable");}
public:
  bool complete() const {return open && sealed && pending.empty();}
  bool shipHistoryReady() const {
    if(!shipMaps)return true;
    for(const auto& r:rasters)if(r && word(ownerBytes(r->scene.view),0x79b)==0x91f8 &&
        gameplayPresentationState(word(ownerBytes(r->scene.view),0x998)) &&
        (!r->shipPrevious || word(ownerBytes(r->shipPrevious),0x79b)!=0x91f8))return false;
    return true;
  }
  void reset() {selector.reset();eye.reset();captured.reset();uploaded.reset();capturedPrevious.reset();uploadedPrevious.reset();lastOrdinaryGraphics.reset();transitionGraphics.reset();pending.clear();open=sealed=false;motherSceneSeen=motherWasExpanded=false;motherPanOffset=0;motherPillar.reset();}
  void configureShip(Bytes maps) {if(open)throw std::logic_error("ship setup during frame");shipMaps=memoryImage(maps,0x272);}
  void configureEnemies(Bytes rom) {if(open)throw std::logic_error("enemy setup during frame");enemyRom=memoryImage(rom,3145728);eye.configureSuit(rom.subspan(0x463c9,128));eye.configurePowerBomb(rom);eye.configureXray(rom);}
  void configureEye(Bytes table) {if(open)throw std::logic_error("eye setup during frame");eye.configure(table);eyeEnabled=true;}
  void powerBombFetch(unsigned address){if(eyeEnabled)eye.powerBombFetch(address);}
  // Ceres Mode7 uses a separate raster renderer, but its actual room/door
  // owners must still advance this selector before the next Mode1 upload.
  void trackScene(uint64_t tag,Bytes bytes) {
    selector.select(std::make_shared<const OwnerImage>(tag,bytes));
    motherSceneSeen=motherWasExpanded=false;motherPanOffset=0;motherPillar.reset();
    eye.reset();captured.reset();uploaded.reset();capturedPrevious.reset();uploadedPrevious.reset();
    lastOrdinaryGraphics.reset();transitionGraphics.reset();pending.clear();open=sealed=false;
  }
  void wramWrite(unsigned address,unsigned value,unsigned width){writable();if(eyeEnabled)eye.write(address,value,width);}
  void begin() {
    if(open && !sealed)throw std::logic_error("unfinished prior frame");
    if(!pending.empty())throw std::logic_error("unpaired prior transaction");
    for(auto& r:rasters)r.reset();
    for(auto& c:changes)c.clear();
    open=true;sealed=false;deferredDisplayError=nullptr;
  }
  void owner(uint64_t tag,Bytes bytes) {
    writable();
    if(eyeEnabled)eye.owner(bytes);
    if(shipMaps)capturedPrevious=uploaded && uploaded->view->tag!=tag?uploaded->view:uploadedPrevious;
    captured=selector.select(std::make_shared<const OwnerImage>(tag,bytes));
    if(captured->kind==SceneKind::Ordinary)transitionGraphics.reset();
    else if(!transitionGraphics)transitionGraphics=lastOrdinaryGraphics;
  }
  void upload(uint64_t tag) {
    writable();if(!captured || captured->view->tag!=tag)throw std::invalid_argument("upload owner mismatch");
    uploaded=captured;uploadedPrevious=capturedPrevious;
  }
  void raster(unsigned v,uint64_t ownerTag,MemoryRef vram,MemoryRef palette,MemoryRef registers) {
    writable();if(v>224 || rasters[v])throw std::invalid_argument("invalid or duplicate raster");
    if(!uploaded || uploaded->view->tag!=ownerTag)throw std::invalid_argument("raster owner mismatch");
    Registers r(memoryBytes(registers,205));
    if(r.get(Reg::display_interlace) || r.get(Reg::display_overscan) || r.get(Reg::pseudoHires) ||
       r.get(Reg::bgMode)==5 || r.get(Reg::bgMode)==6)deferredDisplayError="unsupported center geometry";
    memoryBytes(vram,65536);memoryBytes(palette,512);
    // This callback runs on the emulator's private libco stack. On Windows,
    // even a locally caught C++ exception can escape as STATUS_GCC_THROW.
    // Reject unsupported display effects only later, on the host compose stack.
    std::optional<EyeRow> effect;
    if(eyeEnabled && v>=32)if(auto error=eye.tryRow(r,int(v),effect))deferredDisplayError=error;
    if(v==224 && uploaded->kind==SceneKind::Ordinary && !r.get(Reg::displayDisable) && r.get(Reg::bgMode)==1)
      lastOrdinaryGraphics=std::make_shared<const RetainedRoomGraphics>(RetainedRoomGraphics{vram,palette,r.get(Reg::tiledataAddress)*2,r.get(Reg::bg2_tiledataAddress)*2,word(uploaded->view->bytes,0x79b),word(uploaded->view->bytes,0x7bb),registers});
    rasters[v]=Raster{*uploaded,std::move(vram),std::move(palette),std::move(registers),std::move(effect),uploadedPrevious,transitionGraphics,eyeEnabled?std::optional<bool>(eye.messageOverlay()):std::nullopt};
  }
  void memoryWrite(CommittedWrite w) {
    writable();unsigned size=w.space==1?65536:w.space==2?544:w.space==3?512:0;
    if(!size || (w.width!=1 && w.width!=2) || w.address>size-w.width || w.value>=(1u<<(w.width*8)))
      throw std::invalid_argument("invalid stream memory write");
    pending.push_back(w);
  }
  void registerWrite(uint64_t cycle,unsigned v,unsigned h,unsigned address,MemoryRef registers) {
    writable();memoryBytes(registers,205);
    if(v>=313 || h>=1368 || address<0x2100 || address>0x213f)throw std::invalid_argument("invalid register event");
    unsigned space=address==0x2104?2:(address==0x2118 || address==0x2119)?1:address==0x2122?3:0;
    for(auto w:pending)if(w.cycle!=cycle || w.space!=space)
      throw std::invalid_argument("stream transaction mismatch");
    if(v>=32 && v<=224 && h<=1078)
      changes[v].push_back({cycle,h,address,std::move(registers),std::move(pending)});
    pending.clear();
  }
  void seal() {
    writable();if(!pending.empty())throw std::invalid_argument("unpaired frame write");
    for(const auto& r:rasters)if(!r)throw std::invalid_argument("incomplete raster frame");
    auto view=ownerBytes(rasters[32]->scene.view);
    bool mother=word(view,0x79b)==0xdd58 && word(view,0x7bb)==0xdd6e && word(view,0x911)==0;
    bool expanded=mother && smpMotherBrainArena([&](unsigned a){return word(view,a);});
    // The native tube-clearing event still owns the arena expansion. Ease the
    // presentation's43px reframe over43frames instead of snapping the camera.
    // Loading an already-expanded checkpoint starts at its settled framing.
    if(expanded && motherSceneSeen && !motherWasExpanded)motherPanOffset=43;
    else if(expanded && motherPanOffset)motherPanOffset--;
    else if(!expanded)motherPanOffset=0;
    motherSceneSeen=mother;motherWasExpanded=expanded;
    motherPillar.observe(view);
    sealed=true;
  }
  WideFrame compose(Bytes native,bool blur,RowExecutor* executor=nullptr,const uint32_t* native32=nullptr,unsigned stride=0) const {
    if(!sealed)throw std::logic_error("frame is not sealed");
    if(deferredDisplayError)throw std::invalid_argument(deferredDisplayError);
    if(native32?stride<512:native.size()!=512*480*3)throw std::invalid_argument("native framebuffer size");
    WideFrame result{std::vector<uint8_t>(342*224*3),0};
    result.blur=blur;
    result.centerMask.resize(256*224);
    const unsigned count=executor?executor->count():1;
    std::array<uint64_t,4> clipped{};
    std::array<bool,4> filledCenter{};
    auto composeRows=[&](unsigned worker) {
    SpriteContextCache objectCache; // one cache per disjoint row range
    unsigned first=worker?31+(193*worker)/count:0;
    unsigned last=31+(193*(worker+1))/count;
    for(unsigned y=first;y<last;y++) {
      const bool arena=y>=31 && word(ownerBytes(rasters[y+1]->scene.view),0x911)==0 &&
         (smpMotherBrainArena([&](unsigned a){return word(ownerBytes(rasters[y+1]->scene.view),a);}) ||
          smpCeresArena([&](unsigned a){return word(ownerBytes(rasters[y+1]->scene.view),a);}));
      const unsigned nativeOffset=arena?(word(ownerBytes(rasters[y+1]->scene.view),0x79b)==0xdd58?motherPanOffset:0):43;
      if(native32) {
        const auto* arow=native32+(y+8)*2*stride;
        const auto* brow=arow+stride;
        for(unsigned x=0;x<512;x++)if((arow[x]^brow[x])&0xffffff)
          throw std::invalid_argument("native rows are not duplicated");
        for(unsigned x=0;x<256;x++) {
          uint32_t a=arow[x*2]&0xffffff,b=arow[std::min(x+1,255u)*2]&0xffffff;
          uint32_t expected=blur?(a&b)+(((a^b)&0xfefefe)>>1):a;
          if((arow[x*2+1]&0xffffff)!=expected)throw std::invalid_argument("native blur mapping");
          auto* p=result.rgb.data()+(y*342+x+nativeOffset)*3;
          p[0]=a>>16;p[1]=a>>8;p[2]=a;
        }
      } else {
      unsigned start=(y+8)*2*512*3;
      if(!std::equal(native.begin()+start,native.begin()+start+1536,native.begin()+start+1536))
        throw std::invalid_argument("native rows are not duplicated");
      for(unsigned x=0;x<256;x++)for(unsigned c=0;c<3;c++) {
        auto a=native[start+x*6+c],b=native[start+std::min(x+1,255u)*6+c];
        if(native[start+x*6+3+c]!=(blur?(unsigned(a)+b)/2:a))throw std::invalid_argument("native blur mapping");
        result.rgb[(y*342+x+nativeOffset)*3+c]=a;
      }
      }
      unsigned v=y+1;
      // The HUD is gone during the isolated Samus death animation. Its
      // backdrop/flash fills the entire canvas, including the top margins.
      auto game=word(ownerBytes(rasters[v]->scene.view),0x998);
      if(v<32 && !(game>=0x15 && game<=0x19))continue;
      const auto& r=*rasters[v];RowTimeline row(r.scene,r.vram,r.palette,*r.registers,v,r.eye,shipMaps,r.shipPrevious,enemyRom,r.departing,r.message,&objectCache);
      for(const auto& change:changes[v]) {
        for(auto write:change.writes)row.memoryWrite(write);
        row.registerWrite(change.cycle,change.h,change.address,*change.registers);
      }
      row.finish();
      if(arena && word(ownerBytes(r.scene.view),0x79b)==0xe0b5)for(int x=0;x<256;x++) {
        if(!row.ceresUICandidate(x))continue;
        auto base=row.pixel(x,false,0),filled=row.pixel(x,true,43);
        auto at=(y*342+x)*3;
        if(!base.valid || !filled.valid || !std::equal(base.rgb.begin(),base.rgb.end(),result.rgb.begin()+at))continue;
        filledCenter[worker]=true;result.centerMask[y*256+x]=1;
        std::copy(filled.rgb.begin(),filled.rgb.end(),result.rgb.begin()+at);
      }
      auto centerRange=row.centerRange();
      for(int x=centerRange.first;x<centerRange.second;x++) {
        if(!row.centerCandidate(x))continue;
        auto base=row.pixel(x),filled=row.pixel(x,true);
        auto at=(y*342+x+nativeOffset)*3;
#ifdef SMP_DIAGNOSTIC_CROCOMIRE_REFERENCE
        if(base.valid){filledCenter[worker]=true;result.centerMask[y*256+x]=1;
          std::copy(base.rgb.begin(),base.rgb.end(),result.rgb.begin()+at);continue;}
#endif
        if(!base.valid || !filled.valid || base.rgb==filled.rgb ||
            !std::equal(base.rgb.begin(),base.rgb.end(),result.rgb.begin()+at))continue;
        filledCenter[worker]=true;
        result.centerMask[y*256+x]=1;
        std::copy(filled.rgb.begin(),filled.rgb.end(),result.rgb.begin()+at);
      }
      for(int x=arena?-int(nativeOffset):-43;x<(arena?342-int(nativeOffset):299);x++) {
        if(x>=0 && x<256)continue;
        auto pixel=(arena && word(ownerBytes(r.scene.view),0x79b)==0xe0b5 && row.ceresUICandidate(x))?row.pixel(x,true,43):row.pixel(x);
        if(!pixel.valid) {clipped[worker]++;continue;}
        for(unsigned c=0;c<3;c++)result.rgb[(y*342+unsigned(x+nativeOffset))*3+c]=pixel.rgb[c];
      }
      // The expanded arena uses world0..341, keeping all coordinates positive.
      // Reframe the preserved native pixels at the left; the HUD stays centered.
      // Mask2 tells the presentation filter this entire row has a new origin.
      if(arena){filledCenter[worker]=true;std::fill_n(result.centerMask.begin()+y*256,256,2);}
    }
    };
    if(executor)executor->run(composeRows);else composeRows(0);
    bool hasCenter=false;
    for(unsigned i=0;i<count;i++){result.clipped+=clipped[i];hasCenter|=filledCenter[i];}
    motherPillar.retain(result.rgb);
    if(motherPillar.active() && enemyRom) {
      for(unsigned y=31;y<224;y++) {
        const auto& raster=*rasters[y+1];auto view=ownerBytes(raster.scene.view);
        unsigned offset=smpMotherBrainArena([&](unsigned a){return word(view,a);})?motherPanOffset:43;
        motherPillar.row(result.rgb,y,*enemyRom,*raster.vram,*raster.palette,*raster.registers,int(offset));
        std::fill_n(result.centerMask.begin()+y*256,256,2);
      }
      motherPillar.shake(result.rgb);hasCenter=true;
    }
    if(!hasCenter)result.centerMask.clear();
    return result;
  }
};
}
