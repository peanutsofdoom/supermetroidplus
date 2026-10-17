// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include "../native/frame_pipeline.hpp"
#include "../native/title_scene.hpp"
#include "../native/cinematic_scene.hpp"
#include "../native/ceres_cinematic_wall.hpp"
#include <cstring>
namespace {
struct Cache {uint64_t version=0;SMPWide::MemoryRef data;};
struct Context {
  SMPWide::EyeState cinematicEye;
  SMPWide::CinematicStars cinematicStars;
  SMPWide::CeresCinematicWall ceresWall;
  SMPWide::RowExecutor rows;
  std::unique_ptr<SMPWide::FramePipeline> pipeline=std::make_unique<SMPWide::FramePipeline>();
  std::array<Cache,3> cache;
  SMPWide::WideFrame output;
  std::array<char,256> error{};
  SMPWide::MemoryRef image(unsigned slot,SMPWideImage value,size_t size) {
    if(!value.data || value.size!=size)throw std::invalid_argument("bridge image shape");
    auto& entry=cache[slot];
    if(entry.data && entry.version==value.version) {
      if(!std::equal(entry.data->begin(),entry.data->end(),value.data))throw std::invalid_argument("image changed without version update");
      return entry.data;
    }
    entry={value.version,SMPWide::memoryImage(SMPWide::Bytes(value.data,value.size),size)};
    return entry.data;
  }
  SMPWide::MemoryRef retained(unsigned slot,SMPWideImage value,size_t size) {
    if(value.data)return image(slot,value,size);
    const auto& entry=cache[slot];
    if(value.size!=size || !entry.data || entry.version!=value.version)
      throw std::invalid_argument("retained image version/shape unavailable");
    return entry.data;
  }
};
template<class Action> int guard(void* pointer,Action action) noexcept {
  if(!pointer)return 0;
  auto& context=*static_cast<Context*>(pointer);
  try {action(context);context.error[0]=0;return 1;}
  catch(const std::exception& e) {std::strncpy(context.error.data(),e.what(),context.error.size()-1);context.error.back()=0;return 0;}
  catch(...) {std::strcpy(context.error.data(),"unknown renderer failure");return 0;}
}
}
extern "C" {
int smpwide_cinematic_configure(void* pointer,const uint8_t* rom,size_t bytes) {
  return guard(pointer,[&](Context& c){if(!rom || bytes!=3145728)throw std::invalid_argument("cinematic ROM shape");c.cinematicEye.configurePowerBomb({rom,bytes});c.cinematicStars.configure({rom,bytes});c.ceresWall.configure({rom,bytes});});
}
int smpwide_cinematic_owner(void* pointer,SMPWideImage ram,int reset) {
  return guard(pointer,[&](Context& c){if(!ram.data || ram.size!=131072)throw std::invalid_argument("cinematic owner shape");if(reset){c.cinematicEye.reset();c.cinematicStars.reset();}c.cinematicEye.owner({ram.data,ram.size});c.ceresWall.observe({ram.data,ram.size});});
}
int smpwide_cinematic_stars(void* pointer,SMPWideImage ram) {
  return guard(pointer,[&](Context& c){if(!ram.data || ram.size!=131072)throw std::invalid_argument("cinematic stars owner shape");c.cinematicStars.observe({ram.data,ram.size});});
}
int smpwide_cinematic_write(void* pointer,unsigned address,unsigned width,unsigned value) {
  return guard(pointer,[&](Context& c){c.cinematicEye.write(address,value,width);});
}
int smpwide_cinematic_fetch(void* pointer,unsigned address) {
  return guard(pointer,[&](Context& c){c.cinematicEye.powerBombFetch(address);});
}
int smpwide_cinematic_scene_row(void* pointer,SMPWideImage v,SMPWideImage p,SMPWideImage o,SMPWideImage r,unsigned line,unsigned function,unsigned scene,uint8_t* rgb) {
  // Called on the emulator's private coroutine stack: known display rejection
  // must return directly, rather than throwing across the Windows stack switch.
  if(!pointer)return 0;
  auto& c=*static_cast<Context*>(pointer);
  auto reject=[&](const char* error){std::strncpy(c.error.data(),error,c.error.size()-1);c.error.back()=0;return 0;};
  if(!v.data || v.size!=65536 || !p.data || p.size!=512 || !o.data || o.size!=544 ||
     !r.data || r.size!=205 || !rgb || line<1 || line>224)return reject("cinematic row images");
  std::optional<SMPWide::EyeRow> effect;
  SMPWide::Registers registers({r.data,r.size});
  if(!registers.get(SMPWide::Reg::displayDisable))
    if(auto error=c.cinematicEye.tryRow(registers,line,effect)) {
      int left=registers.get(SMPWide::Reg::window_twoLeft),right=registers.get(SMPWide::Reg::window_twoRight);
      // Ceres keeps its now-closed window configuration after the native PB
      // cleanup. Preserve that native endpoint, with no invented margin beam.
      if(scene==2 && c.cinematicEye.ceresPowerBombTail() && left>=right)
        effect=SMPWide::EyeRow{{},int(line),left,right,false,1,0,true};
      else return reject(error);
    }
  if(!SMPWide::cinematicRow({v.data,v.size},{p.data,p.size},{o.data,o.size},{r.data,r.size},line,function,rgb,scene,effect,scene==4?c.ceresWall.objects():c.cinematicStars.objects()))
    return reject("unsupported cinematic raster");
  c.error[0]=0;return 1;
}
int smpwide_cinematic_row(SMPWideImage v,SMPWideImage p,SMPWideImage o,SMPWideImage r,unsigned line,unsigned function,uint8_t* rgb) {
  if(!v.data || !p.data || !o.data || !r.data)return 0;
  return SMPWide::cinematicRow({v.data,v.size},{p.data,p.size},{o.data,o.size},{r.data,r.size},line,function,rgb);
}
int smpwide_title_row(SMPWideImage v,SMPWideImage p,SMPWideImage r,unsigned line,uint8_t* rgb) {
  if(!v.data || !p.data || !r.data)return 0;
  return SMPWide::titleRow({v.data,v.size},{p.data,p.size},{r.data,r.size},line,rgb);
}
void* smpwide_create() {try{return new Context;}catch(...){return nullptr;}}
void smpwide_destroy(void* pointer) {delete static_cast<Context*>(pointer);}
int smpwide_reset(void* pointer) {return guard(pointer,[](Context& c){c.pipeline=std::make_unique<SMPWide::FramePipeline>();c.cache={};c.output={};});}
int smpwide_reset_history(void* pointer) {return guard(pointer,[](Context& c){c.pipeline->reset();c.cache={};c.output={};});}
int smpwide_track_scene(void* pointer,SMPWideImage ram) {
  return guard(pointer,[&](Context& c){
    if(!ram.data || ram.size!=131072)throw std::invalid_argument("tracked scene shape");
    c.pipeline->trackScene(ram.version,{ram.data,ram.size});c.cache={};
  });
}
int smpwide_configure_eye(void* pointer,const uint8_t* table,size_t bytes) {
  return guard(pointer,[&](Context& c){if(!table || bytes!=256)throw std::invalid_argument("eye table shape");c.pipeline->configureEye(SMPWide::Bytes(table,bytes));});
}
int smpwide_configure_ship(void* pointer,const uint8_t* maps,size_t bytes) {
  return guard(pointer,[&](Context& c){if(!maps || bytes!=0x272)throw std::invalid_argument("ship table shape");c.pipeline->configureShip(SMPWide::Bytes(maps,bytes));});
}
int smpwide_ship_history_ready(void* pointer) {return pointer && static_cast<Context*>(pointer)->pipeline->shipHistoryReady();}
int smpwide_configure_enemies(void* pointer,const uint8_t* rom,size_t bytes) {
  return guard(pointer,[&](Context& c){if(!rom || bytes!=3145728)throw std::invalid_argument("enemy ROM shape");c.pipeline->configureEnemies(SMPWide::Bytes(rom,bytes));});
}
int smpwide_prime_frame(void* pointer) {return guard(pointer,[](Context& c){c.pipeline->seal();});}
int smpwide_powerbomb_fetch(void* pointer,unsigned address) {if(!pointer)return 0;static_cast<Context*>(pointer)->pipeline->powerBombFetch(address);return 1;}
int smpwide_wram_write(void* pointer,unsigned address,unsigned width,unsigned value) {
  return guard(pointer,[&](Context& c){c.pipeline->wramWrite(address,value,width);});
}
int smpwide_begin(void* pointer) {return guard(pointer,[](Context& c){c.pipeline->begin();});}
int smpwide_owner(void* pointer,SMPWideImage wram) {
  return guard(pointer,[&](Context& c){if(!wram.data || wram.size!=131072)throw std::invalid_argument("bridge owner shape");
    c.pipeline->owner(wram.version,SMPWide::Bytes(wram.data,wram.size));});
}
int smpwide_upload(void* pointer,uint64_t owner) {return guard(pointer,[&](Context& c){c.pipeline->upload(owner);});}
int smpwide_raster(void* pointer,unsigned line,uint64_t owner,SMPWideImage vram,SMPWideImage palette,SMPWideImage registers) {
  return guard(pointer,[&](Context& c){c.pipeline->raster(line,owner,c.image(0,vram,65536),c.image(1,palette,512),c.image(2,registers,205));});
}
int smpwide_raster_retained(void* pointer,unsigned line,uint64_t owner,SMPWideImage vram,SMPWideImage palette,SMPWideImage registers) {
  return guard(pointer,[&](Context& c){c.pipeline->raster(line,owner,c.retained(0,vram,65536),c.retained(1,palette,512),c.image(2,registers,205));});
}
int smpwide_memory_write(void* pointer,uint64_t cycle,unsigned space,unsigned address,unsigned width,unsigned value) {
  return guard(pointer,[&](Context& c){c.pipeline->memoryWrite({cycle,space,address,width,value});});
}
int smpwide_register_write(void* pointer,uint64_t cycle,unsigned line,unsigned h,unsigned address,SMPWideImage registers) {
  return guard(pointer,[&](Context& c){c.pipeline->registerWrite(cycle,line,h,address,c.image(2,registers,205));});
}
int smpwide_compose(void* pointer,const uint8_t* native,size_t bytes,int blur) {
  return guard(pointer,[&](Context& c){
    if(!native || bytes!=512*480*3 || (blur!=0 && blur!=1))throw std::invalid_argument("bridge native frame shape");
    c.pipeline->seal();c.output=c.pipeline->compose(SMPWide::Bytes(native,bytes),blur!=0,&c.rows);
  });
}
int smpwide_compose32(void* pointer,const uint32_t* native,unsigned pitch,unsigned width,unsigned height,int blur) {
  return guard(pointer,[&](Context& c){
    if(!native || width!=512 || height!=480 || pitch<512*4 || pitch%4 || (blur!=0 && blur!=1))
      throw std::invalid_argument("bridge native32 frame shape");
    c.pipeline->seal();c.output=c.pipeline->compose({},blur!=0,&c.rows,native,pitch/4);
  });
}
SMPWideOutput smpwide_output(void* pointer) {
  if(!pointer)return {};
  auto& c=*static_cast<Context*>(pointer);
  return {c.output.rgb.data(),c.output.rgb.size(),342,224,c.output.clipped,c.output.centerMask.data(),c.output.centerMask.size(),c.output.blur};
}
int smpwide_recompose(void* pointer,const uint8_t* native,size_t bytes,int blur) {
  return guard(pointer,[&](Context& c){
    if(!native || bytes!=512*480*3 || (blur!=0 && blur!=1))throw std::invalid_argument("bridge native frame shape");
    c.output=c.pipeline->compose(SMPWide::Bytes(native,bytes),blur!=0,&c.rows);
  });
}
const char* smpwide_error(void* pointer) {return pointer?static_cast<Context*>(pointer)->error.data():"no renderer context";}
}
