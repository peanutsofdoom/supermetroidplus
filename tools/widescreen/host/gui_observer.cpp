// SPDX-License-Identifier: GPL-3.0-or-later
// GUI callback adapter. Rendering errors are counted; guest execution continues.
#include <sfc/sfc.hpp>
#undef platform
#include "bridge.h"
#include "gui_observer.h"
#include "../../playtest/recorder.h"
#include "presentation.hpp"
#include "tv_presentation.hpp"
#include "scene_eligibility.hpp"
#include "playthrough_eligibility.hpp"
#include "experimental_activation.hpp"
#include "crocomire_scare.hpp"
#include "mother_brain_arena.hpp"
#include "ceres_arena.hpp"
#include "cinematic_eligibility.hpp"
#include "coordinate_overlay.hpp"
#include "fps_meter.hpp"
#include "title_reveal.hpp"
#include "retest_queue_overlay.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef SMP_COORDINATE_BUILD
#define SMP_COORDINATE_BUILD 0
#endif
#include <map>
#include <vector>
#include <fstream>
#include <string>
#include <iomanip>
namespace SF=SuperFamicom;
namespace SMPTrace {uint64_t cycles=0;}
namespace {
bool requested=false,known=false,active=false,failed=false;
bool tv=false,displayWide=false;
bool titleActive=false,titleFailed=false;
bool cinematicActive=false,cinematicOamDirty=true;
bool cinematicOwned=false;
unsigned cinematicScene=0;
void* cinematicContext=nullptr;
std::vector<uint8_t> cinematicOam(544);
SmpTitleSkip titleSkip;
std::vector<uint8_t> titleRgb;
std::vector<uint8_t> cinematicMask;
unsigned displayViewportWidth=0,displayViewportHeight=0,displayWidth=0,displayHeight=0;
bool eyeConfigured=false;
void* context=nullptr;
uint64_t owner=0,uploaded=0,vrVersion=0,cgVersion=0,regVersion=0;
uint64_t frames=0,wideFrames=0,unsupported=0,failures=0;
uint64_t titleWideFrames=0,timedFrames=0;
uint64_t cinematicWideFrames=0;
uint64_t introWideFrames=0,ceresMode7WideFrames=0;
SmpFpsMeter fpsMeter;
double emulationStart=0,presentStart=0,framePresented=0,emulationSeconds=0,composeSeconds=0,presentSeconds=0,lastReportTime=0;
double wallTime(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct ComposeTiming {double start=wallTime();~ComposeTiming(){composeSeconds+=wallTime()-start;}};
bool vrDirty=true,cgDirty=true;
std::vector<uint8_t> vr(65536),cg(512),regs(205),ownerRam,rgb;
std::vector<uint8_t> eyeTable,shipMaps;
std::vector<uint8_t> enemyRom;
std::vector<uint32_t> output;
std::string error;
#ifdef SMP_EXPERIMENTAL_PLAYTHROUGH
SMPPlaythroughEligibility eligibility;
#else
SMPGuiEligibility eligibility;
#endif
struct SceneStats {uint64_t attempted=0,wide=0,failed=0;std::string lastError;};
std::map<unsigned,SceneStats> sceneStats;
unsigned statsKey=0;
bool activationRomKnown=false;
uint64_t activationChecks=0,activationExpanded=0;
unsigned word(unsigned a){return SF::cpu.wram[a]|unsigned(SF::cpu.wram[a+1])<<8;}
bool check(int ok) {
  if(ok)return true;
  if(!failed){failures++;error=smpwide_error(context);sceneStats[statsKey].failed++;sceneStats[statsKey].lastError=error;}
  failed=true;return false;
}
bool cinematicCheck(int ok) {
  if(ok)return true;
  titleFailed=true;error=smpwide_error(cinematicContext);return false;
}
SMPWideImage imagePacket(uint64_t v,const std::vector<uint8_t>& data){return {v,data.data(),data.size()};}
SMPWideImage registers(bool now) {
  if(now) {
    // The diagnostic schema is fixed for this executable. Check it once, then
    // reuse the save buffer instead of counting and allocating at every write.
    static const unsigned schema=[](){nall::serializer count;SF::ppu.diagnosticPresentationRegisters(count);return count.size();}();
    if(schema!=205){failed=true;error="register schema";return imagePacket(regVersion,regs);}
    static nall::serializer data(schema);
    data.setMode(nall::serializer::Save);SF::ppu.diagnosticPresentationRegisters(data);
    if(data.size()!=205){failed=true;error="register schema";return imagePacket(regVersion,regs);}
    if(!std::equal(regs.begin(),regs.end(),data.data())){regs.assign(data.data(),data.data()+data.size());regVersion++;}
  }
  return imagePacket(regVersion,regs);
}
void resetStream() {
  owner=uploaded=vrVersion=cgVersion=regVersion=0;vrDirty=cgDirty=true;regs.assign(205,0);ownerRam.clear();
}
void discard(bool retainAssets=true) {
  eligibility.reset();
  if(context) {
    if(retainAssets && eyeConfigured)smpwide_reset_history(context);
    else smpwide_reset(context);
  }
  if(!retainAssets)eyeConfigured=false;
  resetStream();
}
}
void smpgui_emulation_done(){emulationSeconds+=std::max(0.0,wallTime()-emulationStart-framePresented);timedFrames++;}
void smpgui_present_begin(){presentStart=wallTime();}
void smpgui_present_done(){double elapsed=wallTime()-presentStart;presentSeconds+=elapsed;framePresented+=elapsed;}
bool smpgui_report_due(){double now=wallTime();if(!lastReportTime){lastReportTime=now;return false;}return now-lastReportTime>=10;}
void smpgui_request(bool value){requested=value;}
void smpgui_tv(bool value){tv=value;}
bool reduceSlowdown=false;
void smpgui_reduce_slowdown(bool value){reduceSlowdown=value;}
bool smpgui_known(){return known;}
void smpgui_display(unsigned vw,unsigned vh,unsigned ow,unsigned oh,bool wide) {
  displayViewportWidth=vw;displayViewportHeight=vh;displayWidth=ow;displayHeight=oh;displayWide=wide;
}
SmpCoordinateUnderlay coordinateUnderlay;
unsigned coordinateWidth=0,coordinateHeight=0;
void smpgui_clear_coordinates() {
  if(output.size()==size_t(coordinateWidth)*coordinateHeight)
    coordinateUnderlay.restore(output.data(),coordinateWidth,coordinateHeight);
  else coordinateUnderlay.reset();
}
const uint32_t* smpgui_coordinates(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,unsigned crop,bool wide) {
  static std::vector<uint32_t> surface;
  char first[100],second[100],rate[24];
  std::snprintf(first,sizeof(first),"D%u%s R:%04X S:%04X G:%02X %c F:%06llu",unsigned(SMP_COORDINATE_BUILD),reduceSlowdown && known && SF::configuration.hacks.cpu.overclock==125?"B":"",word(0x79b),word(0x7bb),word(0x998),wide?'W':'N',static_cast<unsigned long long>(frames%1000000));
  if(fpsMeter.fps()<0)std::snprintf(rate,sizeof(rate),"--.-");
  else std::snprintf(rate,sizeof(rate),"%.1f",fpsMeter.fps());
  std::snprintf(second,sizeof(second),"P:%05u,%05u C:%05u,%05u FPS:%s",word(0xaf6),word(0xafa),word(0x911),word(0x915),rate);
  // The composed display surface is ours; avoid copying its entire frame
  // solely to draw telemetry. Native emulator framebuffers still use a copy.
  if(data==output.data() && pitch==width*4 && output.size()==size_t(width)*height) {
    smpgui_clear_coordinates();
    coordinateWidth=width;coordinateHeight=height;
    coordinateUnderlay.capture(output.data(),width,height,crop);
    smpDrawCoordinatePanel(output.data(),width,height,crop,first,second);return output.data();
  }
  return smpCoordinateSurface(data,pitch,width,height,crop,first,second,surface);
}
const uint32_t* smpgui_queue_surface(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,unsigned crop,const char* code,unsigned seconds,bool complete,bool error) {
  static std::vector<uint32_t> surface;
  uint32_t* target=nullptr;
  if(data==output.data() && pitch==width*4 && output.size()==size_t(width)*height)target=output.data();
  else {surface.resize(size_t(width)*height);for(unsigned y=0;y<height;y++)std::copy(data+y*(pitch/4),data+y*(pitch/4)+width,surface.begin()+size_t(y)*width);target=surface.data();}
  smpDrawQueuePanel(target,width,height,crop,code,seconds,complete,error);return target;
}
void smpgui_load(const uint8_t* rom,size_t bytes) {
  smpgui_reset();
  known=std::string((const char*)nall::Hash::SHA256({rom,bytes}).digest())=="12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72";
  activationRomKnown=known;
  if(known)eyeTable.assign(rom+0x8c9d4,rom+0x8cad4);
  if(known)shipMaps.assign(rom+0x112d81,rom+0x112ff3);
#ifdef SMP_EXPERIMENTAL_ENEMIES
  if(known)enemyRom.assign(rom,rom+bytes);
#endif
}
void smpgui_reset(){coordinateUnderlay.reset();titleSkip.reset();fpsMeter.reset();titleActive=false;titleFailed=false;cinematicActive=false;cinematicOwned=false;cinematicScene=0;if(cinematicContext){smpwide_destroy(cinematicContext);cinematicContext=nullptr;}titleRgb.clear();discard(false);active=false;failed=false;known=false;eyeTable.clear();shipMaps.clear();enemyRom.clear();output.clear();}
bool smpgui_begin(bool compatible) {
  coordinateUnderlay.reset();
  // Opt-in profiling only; never feeds timing back into guest execution.
  static FILE* trace=[](){const char* path=std::getenv("SMP_FRAME_TIMING");return path?std::fopen(path,"w"):nullptr;}();
  static double previousBegin=0,previousEmulation=0,previousCompose=0,previousPresent=0;
  if(trace) {
    const double now=wallTime();
    if(previousBegin)std::fprintf(trace,"%llu\t%u\t%.6f\t%.6f\t%.6f\t%.6f\n",(unsigned long long)frames,word(0x998),(now-previousBegin)*1000,(emulationSeconds-previousEmulation)*1000,(composeSeconds-previousCompose)*1000,(presentSeconds-previousPresent)*1000);
    previousBegin=now;previousEmulation=emulationSeconds;previousCompose=composeSeconds;previousPresent=presentSeconds;
    if(frames%60==0)std::fflush(trace);
  }
  emulationStart=wallTime();framePresented=0;fpsMeter.sample(++frames,emulationStart);active=false;failed=false;titleActive=false;titleFailed=false;cinematicActive=false;
  if(!requested || !known){cinematicOwned=false;return false;}
  bool supported=eligibility.allow(word(0x998),word(0x99c),word(0x79b),word(0x7bb));
  statsKey=(word(0x79b)<<16)|word(0x7bb);
  if(supported)sceneStats[statsKey].attempted++;
  auto& h=SF::configuration.hacks;
  bool native=(h.cpu.overclock==100 || (reduceSlowdown && h.cpu.overclock==125)) && !h.cpu.fastMath && !h.cpu.fastJoypadPolling &&
    !h.ppu.deinterlace && !h.ppu.noSpriteLimit && !h.ppu.noVRAMBlocking && h.ppu.mode7.scale==1 &&
    h.ppu.mode7.widescreen==0 && !h.dsp.fast && !h.dsp.cubic && !h.dsp.echoShadow &&
    !h.coprocessor.delayedSync && !h.coprocessor.preferHLE;
#ifdef SMP_EXPERIMENTAL_PLAYTHROUGH
  cinematicScene=smpCinematicScene(word(0x998),word(0x1f51),word(0x79b),word(0x56));
  if(cinematicScene==3 && smpCeresArena([](unsigned a){return word(a);}))cinematicScene=4;
  if(compatible && native && !SF::system.fastPPU() && cinematicScene) {
    if(cinematicScene>=3) {
      // Retain real Ceres door history across the Mode7/Mode1 handoff. Clearing
      // it here used to force the whole destination fade back to4:3.
      if(!context)context=smpwide_create();
      unsigned state=word(0x998);
      if(context && (state==7 || state==8 || state==9 || state==11))
        if(!smpwide_track_scene(context,{frames,SF::cpu.wram,131072}))eligibility.reset();
      resetStream();
    }else discard();
    titleActive=cinematicActive=true;cinematicOamDirty=true;titleRgb.assign(342*224*3,0);
    if(!cinematicContext) {
      cinematicContext=smpwide_create();
      cinematicCheck(smpwide_cinematic_configure(cinematicContext,enemyRom.data(),enemyRom.size()));
    }
    cinematicCheck(smpwide_cinematic_owner(cinematicContext,{0,SF::cpu.wram,131072},!cinematicOwned));
    cinematicOwned=true;return true;
  }
  cinematicOwned=false;
  if(compatible && native && !SF::system.fastPPU() && (word(0x998)==0 || word(0x998)==1 || word(0x998)==0x28)) {
    discard();titleActive=true;titleRgb.assign(342*224*3,0);return true;
  }
#endif
  if(!compatible || !native || SF::system.fastPPU() || !supported){unsupported++;discard();return false;}
  if(!context)context=smpwide_create();
  if(!context){failures++;error="renderer allocation";return false;}
  if(!eyeConfigured) {
    if(!check(smpwide_configure_eye(context,eyeTable.data(),eyeTable.size()))){discard();return false;}
    if(!check(smpwide_configure_ship(context,shipMaps.data(),shipMaps.size()))){discard();return false;}
#ifdef SMP_EXPERIMENTAL_ENEMIES
    if(!check(smpwide_configure_enemies(context,enemyRom.data(),enemyRom.size()))){discard();return false;}
#endif
    eyeConfigured=true;
  }
  // Each scheduler frame obtains current native memory even if observation
  // was inactive across a menu/reset boundary.
  vrDirty=cgDirty=true;active=check(smpwide_begin(context));return active;
}
// The GUI queues native video only while this is true, then calls finish().
// Title composition has no gameplay upload owner but still needs that queue.
bool smpgui_active(){return active || titleActive;}
const uint32_t* smpgui_finish(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,bool compactTv) {
  ComposeTiming compositionTimer;
  titleSkip.observe(word(0x998),word(0x1f51),word(0x1a53));
  if(titleActive) {
    titleActive=false;
    if(titleFailed || width!=512 || height!=480){failures++;if(!titleFailed)error=cinematicActive?"unsupported cinematic presentation":"unsupported title presentation";return nullptr;}
    SMPWideOutput w{titleRgb.data(),titleRgb.size(),342,224,0};
    if(cinematicActive && cinematicScene==4) {
      cinematicMask.assign(256*224,0);
      for(unsigned y=31;y<224;y++) {
        std::fill_n(cinematicMask.begin()+y*256,256,2);
#ifndef SMP_CINEMATIC_VALIDATE_CENTER
        for(unsigned x=0;x<256;x++) {
          unsigned value=data[(y+8)*2*(pitch/4)+x*2];
          for(unsigned c=0;c<3;c++)titleRgb[(y*342+x)*3+c]=value>>(16-c*8);
        }
#endif
      }
      w.center_mask=cinematicMask.data();w.center_mask_size=cinematicMask.size();
    }
    if(tv && compactTv)smpGuiCompactTvSurface(data,pitch,width,height,w,output);
    else if(tv)smpGuiTvSurface(data,pitch,width,height,w,output);
    else output=smpGuiSurface(data,pitch,width,height,w);
#ifndef SMP_TEST_NO_TITLE_REVEAL
    if(!cinematicActive)smpTitleReveal(output,tv?(compactTv?684:2048):684,tv?(compactTv?224:448):480,
      tv?0:16,word(0x998)==1?word(0x1f51):0,word(0x1f53),word(0x51)&255,titleSkip.skipped);
#endif
    wideFrames++;if(cinematicActive){if(cinematicScene==1)cinematicWideFrames++;else if(cinematicScene==2)introWideFrames++;else ceresMode7WideFrames++;}else titleWideFrames++;return output.data();
  }
  if(!active)return nullptr;
  active=false;
  if(failed || width!=512 || height!=480){if(!failed){failures++;error="native geometry";}discard();return nullptr;}
  if(!smpwide_ship_history_ready(context)) {
    // Present this original frame while retaining read-only owner history.
    // No extra emulated frame or presentation queue is introduced.
    if(!check(smpwide_prime_frame(context)))discard();
    unsupported++;return nullptr;
  }
  if(!check(smpwide_compose32(context,data,pitch,width,height,SF::configuration.video.blurEmulation))){discard();return nullptr;}
  auto wide=smpwide_output(context);
  if(tv && compactTv)smpGuiCompactTvSurface(data,pitch,width,height,wide,output);
  else if(tv)smpGuiTvSurface(data,pitch,width,height,wide,output);
  else output=smpGuiSurface(data,pitch,width,height,wide);
  wideFrames++;sceneStats[statsKey].wide++;return output.data();
}
void smpgui_report(const char* path) {
  if(!path || !*path)return;
  lastReportTime=wallTime();
  std::ofstream f(path);
  f<<"{\"frames\":"<<frames<<",\"wide_frames\":"<<wideFrames<<",\"unsupported_frames\":"<<unsupported<<",\"failed_frames\":"<<failures
   <<",\"reduce_original_slowdown\":"<<(reduceSlowdown?"true":"false")<<",\"emulated_cpu_percent\":"<<unsigned(SF::configuration.hacks.cpu.overclock)
   <<",\"title_wide_frames\":"<<titleWideFrames
   <<",\"ending_wide_frames\":"<<cinematicWideFrames
   <<",\"intro_wide_frames\":"<<introWideFrames<<",\"ceres_mode7_wide_frames\":"<<ceresMode7WideFrames
   <<",\"fps\":"<<fpsMeter.fps()<<",\"timed_frames\":"<<timedFrames
   <<",\"emulation_ms_total\":"<<emulationSeconds*1000<<",\"compose_ms_total\":"<<composeSeconds*1000
   <<",\"present_ms_total\":"<<presentSeconds*1000
   <<",\"known_rom\":"<<(known?"true":"false")<<",\"requested\":"<<(requested?"true":"false")
   <<",\"tv_layout\":"<<(tv?"true":"false")<<",\"last_display_wide\":"<<(displayWide?"true":"false")
   <<",\"viewport_width\":"<<displayViewportWidth<<",\"viewport_height\":"<<displayViewportHeight
   <<",\"output_width\":"<<displayWidth<<",\"output_height\":"<<displayHeight
   <<",\"last_error\":"<<std::quoted(error)
#ifdef SMP_EXPERIMENTAL_PLAYTHROUGH
   <<",\"experimental_playthrough\":true"
#else
   <<",\"experimental_playthrough\":false"
#endif
   <<",\"activation_checks\":"<<activationChecks<<",\"activation_expanded_decisions\":"<<activationExpanded
   <<",\"scenes\":[";
  bool first=true;
  for(const auto& entry:sceneStats) {
    if(!first)f<<',';first=false;
    f<<"{\"room\":"<<(entry.first>>16)<<",\"state\":"<<(entry.first&65535)
     <<",\"attempted\":"<<entry.second.attempted<<",\"wide\":"<<entry.second.wide
     <<",\"failed\":"<<entry.second.failed<<",\"last_error\":"<<std::quoted(entry.second.lastError)<<'}';
  }
  f<<"]}\n";
}
namespace SMPTrace {
void bus(unsigned address,unsigned value,bool writing) {
  smpr_bus(address,value,writing);
  if(!active || failed || !writing || ((address>>16)&127)>=64 || (address&0xffc0)!=0x2100)return;
  unsigned v=SF::ppu.vcounter(),h=SF::ppu.hcounter();
  check(smpwide_register_write(context,cycles,v,h,address&65535,registers(v>=32 && v<=224 && h<=1078)));
}
void write(const char* name,unsigned address,unsigned value,unsigned width) {
  smpr_write(name,address,value,width);
  if(titleActive) {
    if(std::strcmp(name,"vram")==0)vrDirty=true;
    if(std::strcmp(name,"cgram")==0)cgDirty=true;
    if(std::strcmp(name,"oam")==0)cinematicOamDirty=true;
    if(cinematicActive && std::strcmp(name,"wram")==0)cinematicCheck(smpwide_cinematic_write(cinematicContext,address,width,value));
  }
  if(!active || failed)return;
  if(std::strcmp(name,"wram")==0) {check(smpwide_wram_write(context,address,width,value));return;}
  unsigned space=std::strcmp(name,"vram")==0?1:std::strcmp(name,"oam")==0?2:std::strcmp(name,"cgram")==0?3:0;
  if(space==1){vrDirty=true;vrVersion++;}if(space==3){cgDirty=true;cgVersion++;}
  if(space)check(smpwide_memory_write(context,cycles,space,address,width,value));
}
void instruction(unsigned pc) {
  if(cinematicActive && cinematicScene==4 && pc==0x809589)
    cinematicCheck(smpwide_cinematic_owner(cinematicContext,{frames,SF::cpu.wram,131072},0));
  if(cinematicActive && cinematicScene==1 && pc==0x8be978)
    cinematicCheck(smpwide_cinematic_stars(cinematicContext,{frames,SF::cpu.wram,131072}));
#ifdef SMP_EXPERIMENTAL_ACTIVATION
#ifndef SMP_TEST_ORIGINAL_CERES_ARENA
  // Expand after destination enemies/PLMs load, while the door fade is black.
  if(activationRomKnown && ((pc==0x828b98 && word(0x998)==8) || (pc==0x82e4dc && word(0x998)==11))) {
    auto byte=[](unsigned a,unsigned v){SF::cpu.write(0x7e0000+a,uint8_t(v));};
    auto write=[&](unsigned a,unsigned v){byte(a,v);byte(a+1,v>>8);};
    smpExpandCeresArena([](unsigned a){return word(a);},write,byte);
    smpMoveCeresWallActor([](unsigned a){return word(a);},write);
  }
  if(activationRomKnown && pc==0x90b17c && !SF::cpu.r.p.m &&
     smpCeresArena([](unsigned a){return word(a);})) {
    unsigned a=SF::cpu.r.a.w;SF::cpu.r.p.n=bool(uint16_t(a-400)&0x8000);SF::cpu.r.p.z=a==400;SF::cpu.r.p.c=a>=400;
  }
#endif
#ifndef SMP_TEST_ORIGINAL_MOTHER_ARENA
  // Wait until the native tube-removal PLMs have laid down the clean chamber
  // background. Expanding earlier would copy transient tank/tube artwork.
  if(activationRomKnown && (pc==0xa98d49 || pc==0x828b98) && smpMotherBrainRoom([](unsigned a){return word(a);}) &&
     (pc==0xa98d49 || (word(0x7800)>=2 && word(0x7800)<=4))) {
    auto writeByte=[](unsigned a,unsigned v){SF::cpu.write(0x7e0000+a,uint8_t(v));};
    auto write=[&](unsigned a,unsigned v){writeByte(a,v);writeByte(a+1,v>>8);};
    smpExpandMotherBrainArena([](unsigned a){return word(a);},write,writeByte);
  }
  if(activationRomKnown && !SF::cpu.r.p.m && (pc==0xa9bc5c || pc==0xa9bc68) &&
     smpMotherBrainArena([](unsigned a){return word(a);})) {
    if(pc==0xa9bc68)SF::cpu.r.a.w=315;
    else {unsigned a=SF::cpu.r.a.w;SF::cpu.r.p.n=bool(uint16_t(a-315)&0x8000);SF::cpu.r.p.z=a==315;SF::cpu.r.p.c=a>=315;}
  }
  // Match each attack's own original wall clearance at the relocated wall.
  // Damage, velocity, vertical bounds, and firing scripts remain native.
  if(activationRomKnown && !SF::cpu.r.p.m &&
     (pc==0x86c3e4 || pc==0x86c5d2 || pc==0x86c719 || pc==0x90b17c || pc==0x86e6fa) &&
     smpMotherBrainArena([](unsigned a){return word(a);})) {
    unsigned limit=pc==0x86c3e4?328:pc==0x86c5d2?320:pc==0x86c719?318:pc==0x90b17c?400:336;
    unsigned slot=SF::cpu.r.x.w;
    bool qualified=pc!=0x86e6fa || (slot<36 && !(slot&1) && word(0x1997+slot)==0xa17b);
    if(qualified) {
      unsigned a=SF::cpu.r.a.w;if(pc==0x86e6fa)limit+=word(0x911);
      SF::cpu.r.p.n=bool(uint16_t(a-limit)&0x8000);SF::cpu.r.p.z=a==limit;SF::cpu.r.p.c=a>=limit;
    }
  }
#endif
#ifndef SMP_TEST_NO_CROCOMIRE_SCARE
  if(activationRomKnown && (pc==0x828b98 || pc==0x82db6c)) {
    auto read=[](unsigned a){return word(a);};
    auto write=[](unsigned a,unsigned v){SF::cpu.write(0x7e0000+a,uint8_t(v));SF::cpu.write(0x7e0001+a,uint8_t(v>>8));};
    if(pc==0x82db6c)smpCrocoScareFloor(read,write);
    else if(enemyRom.size()==3145728)smpCrocoScareTick(read,[](unsigned a){return unsigned(enemyRom[a]);},write);
  }
#endif
  // Gameplay compatibility hook, deliberately independent of renderer success
  // and widescreen toggle. The unmodified ROM is identified before enabling it.
#ifndef SMP_TEST_ORIGINAL_CROCOMIRE_BARRIER
  if((pc>>16)==0x94 && activationRomKnown && !SF::cpu.r.p.m &&
     (pc==0x94951f || pc==0x949536 || pc==0x94a1c5 || pc==0x94a1e6 || pc==0x948f54 || pc==0x948f6e) &&
     smpCrocomireBarrierActive([](unsigned a){return word(a);})) {
    unsigned block=word(0xdc4);
    if(pc==0x948f54 || pc==0x948f6e) {
      if(smpCrocomireBarrierOverlap(block,int(word(0x20)),int(word(0x20))+1))
        SF::cpu.r.a.w=pc==0x948f54?811:858;
    } else {
      int left=0,right=0;bool valid=true;
      if(pc==0x94951f){left=int(word(0x20));right=left+1;}
      else if(pc==0x94a1c5){left=int(word(0x1c));right=left+1;}
      else if(pc==0x949536){left=int(word(0xaf6))-int(word(0xafe));right=int(word(0xaf6))+int(word(0xafe));}
      else {
        unsigned slot=word(0xdde);valid=slot<20 && !(slot&1);
        if(valid){left=int(word(0xb64+slot))-int(word(0xbb4+slot));right=int(word(0xb64+slot))+int(word(0xbb4+slot));}
      }
      if(valid)SF::cpu.r.a.w=smpCrocomireBarrierType(block,SF::cpu.r.a.w,left,right);
    }
  }
#endif
#ifndef SMP_TEST_ORIGINAL_CROCOMIRE_LEFT
  if((pc>>16)==0x94 && activationRomKnown && !SF::cpu.r.p.m &&
     (pc==0x94951f || pc==0x949536 || pc==0x94a1c5 || pc==0x94a1e6 ||
      pc==0x948f54 || pc==0x948f6e || pc==0x949052 || pc==0x949064)) {
    unsigned block=word(0xdc4);
    auto wall=smpCrocomireLeftWall([](unsigned a){return word(a);},block);
    if(wall) {
      if(pc==0x948f54 || pc==0x948f6e) {
        if(smpCrocomireLeftOverlap(wall,block,int(word(0x20)),int(word(0x20))+1))
          SF::cpu.r.a.w=pc==0x948f54?wall.left:wall.right-1;
      } else if(pc==0x949052 || pc==0x949064) {
        if(wall.spikes)SF::cpu.r.a.w=word(0x16402+(block/128)*128+34);
      } else {
        int left=0,right=0;bool valid=true;
        if(pc==0x94951f){left=int(word(0x20));right=left+1;}
        else if(pc==0x94a1c5){left=int(word(0x1c));right=left+1;}
        else if(pc==0x949536){left=int(word(0xaf6))-int(word(0xafe));right=int(word(0xaf6))+int(word(0xafe));}
        else {unsigned slot=word(0xdde);valid=slot<20 && !(slot&1);if(valid){left=int(word(0xb64+slot))-int(word(0xbb4+slot));right=int(word(0xb64+slot))+int(word(0xbb4+slot));}}
        if(valid)SF::cpu.r.a.w=smpCrocomireLeftType(wall,block,SF::cpu.r.a.w,left,right);
      }
    }
  }
#endif
  if(activationRomKnown && (pc==0xa08f0a || pc==0xa08f1a) && !SF::cpu.r.p.m) {
    unsigned slot=SF::cpu.r.x.w;
    if(slot<0x800 && !(slot&63)) {
      unsigned rhs=pc==0xa08f0a?word(0x911):word(0xf7a+slot);
      bool negative=smpExpandedActivationNegative(SF::cpu.r.a.w,rhs);
      activationChecks++;if(SF::cpu.r.p.n && !negative)activationExpanded++;
      SF::cpu.r.p.n=negative;
    }
  }
  if(activationRomKnown && (pc==0x86e6e8 || pc==0x86e6fa) && !SF::cpu.r.p.m &&
     word(0x998)==8 && (word(0x79f)==1 || word(0x79b)==0x975c)) {
    unsigned slot=SF::cpu.r.x.w;
    if(slot<0x24 && !(slot&1) && word(0x1997+slot)==0xa17b)
      SF::cpu.r.p.n=smpPirateLaserBoundsNegative(pc,word(0x1a4b+slot),word(0x911));
  }
#ifndef SMP_TEST_ORIGINAL_ZOA_RESET
  if(activationRomKnown && (pc==0xa0ad79 || pc==0xa0ad85) && !SF::cpu.r.p.m &&
     word(0x998)==8 && word(0x79b)==0xd6fd) {
    unsigned slot=SF::cpu.r.x.w,a=0xf78+slot,sp=SF::cpu.r.s.w;
    unsigned caller=sp<0x1ffd ? word(sp+1)|(unsigned(SF::cpu.wram[sp+3])<<16) : 0;
    if(slot<0x800 && !(slot&63) && word(a)==0xda7f && word(a+46)==0xa3 &&
       word(a+60)==0xb4d6 && word(a+10)==8 && word(a+2)<0x8000 &&
       (caller==0xa3b4f1 || caller==0xa3b50e))
      SF::cpu.r.p.n=smpZoaBoundsNegative(pc,word(a+2),word(0x911),word(a+10));
  }
#endif
#ifndef SMP_TEST_ORIGINAL_PIPEBUG_RESET
  if(activationRomKnown && (pc==0xa0c19b || pc==0xa0c1a5) && !SF::cpu.r.p.m && word(0x998)==8) {
    unsigned slot=SF::cpu.r.x.w,a=0xf78+slot;
    bool approvedZebbo=word(a)==0xf1d3 &&
      ((word(0x79b)==0xb482 && word(0x7bb)==0xb48f) || (word(0x79b)==0xb37a && word(0x7bb)==0xb387));
    if(slot<0x800 && !(slot&63) && (word(a)==0xf193 || approvedZebbo) && word(a+46)==0xb3 &&
       word(a+58)==0x891c && word(a+10)==8 && word(a+2)<0x8000)
      SF::cpu.r.p.n=smpPipeBugBoundsNegative(pc,word(a+2),word(0x911),word(a+10));
  }
#endif
  if(activationRomKnown && (pc==0x86dad4 || pc==0x86dae0) && !SF::cpu.r.p.m && word(0x998)==8) {
    unsigned slot=SF::cpu.r.x.w;
    if(slot<0x24 && !(slot&1) && word(0x1997+slot)==0xdafe)
      SF::cpu.r.p.n=smpCacatacSpikeBoundsNegative(pc,word(0x1a4b+slot),word(0x911));
  }
#endif
  if(!active || failed)return;
  if(pc==0x809589 && SF::cpu.wram[0x5b4]) {
    ownerRam.assign(SF::cpu.wram,SF::cpu.wram+131072);check(smpwide_owner(context,imagePacket(++owner,ownerRam)));
  } else if(pc==0x8095a1) {
    std::vector<uint8_t> oam(544),colors(512);
    if(ownerRam.size()!=131072 || !SF::ppu.diagnosticMemory(2,oam.data()) || !SF::ppu.diagnosticMemory(3,colors.data()))
      {failed=true;failures++;error="upload source unavailable";return;}
    for(unsigned i=0;i<544;i++)if(oam[i]!=ownerRam[0x370+i]){failed=true;failures++;error="OAM source differs";return;}
    for(unsigned i=0;i<512;i++)if(colors[i]!=(ownerRam[0xc000+i]&(i&1?127:255)))
      {failed=true;failures++;error="palette source differs";return;}
    if(check(smpwide_upload(context,owner)))uploaded=owner;
  }
}
void raster() {
  if(titleActive) {
    unsigned line=SF::ppu.vcounter();
    if(line<1 || line>224 || (!cinematicActive && word(0x998)!=1))return;
    if(vrDirty){SF::ppu.diagnosticMemory(1,vr.data());vrDirty=false;}
    if(cgDirty){SF::ppu.diagnosticMemory(3,cg.data());cgDirty=false;}
    if(cinematicActive) {
      if(cinematicOamDirty){SF::ppu.diagnosticMemory(2,cinematicOam.data());cinematicOamDirty=false;}
      cinematicCheck(smpwide_cinematic_scene_row(cinematicContext,imagePacket(0,vr),imagePacket(0,cg),imagePacket(0,cinematicOam),registers(true),line,word(0x1f51),cinematicScene,titleRgb.data()));
    }else if(!smpwide_title_row(imagePacket(0,vr),imagePacket(0,cg),registers(true),line,titleRgb.data()))titleFailed=true;
    return;
  }
  if(!active || failed || !uploaded || SF::ppu.vcounter()>224)return;
  const bool freshVram=vrDirty,freshPalette=cgDirty;
  if(vrDirty){SF::ppu.diagnosticMemory(1,vr.data());vrDirty=false;}
  if(cgDirty){SF::ppu.diagnosticMemory(3,cg.data());cgDirty=false;}
  // Check every fresh capture, including the first row of each frame. Other
  // rows reuse the bridge's immutable copy, never mutable guest storage.
  SMPWideImage v{vrVersion,freshVram?vr.data():nullptr,vr.size()};
  SMPWideImage c{cgVersion,freshPalette?cg.data():nullptr,cg.size()};
  check(smpwide_raster_retained(context,SF::ppu.vcounter(),uploaded,v,c,registers(true)));
}
void latch(unsigned port){smpr_latch(port);}
void interrupt(const char* kind,const char* phase){smpr_interrupt(kind,phase);}
void dma(unsigned channel,const char* phase){smpr_dma(channel,phase);}
bool diagnosticDMAStall(unsigned){return false;}
void ownershipWrite(const char*){} void versionWrite(const char*,unsigned,unsigned,unsigned){}
void versionBus(unsigned,unsigned,bool){} void compactInstruction(unsigned){}
void compactSignal(const char*,unsigned,unsigned,unsigned){} void boundary(unsigned,unsigned){}
void signal(const char* name,unsigned channel,unsigned a,unsigned b){
  smpr_signal(name,channel,a,b);
  if(titleActive && cinematicActive && a>=0x7ec406 && a<=0x7ec607 && std::strcmp(name,"hdma-byte")==0)
    cinematicCheck(smpwide_cinematic_fetch(cinematicContext,a));
  if(active && !failed && ((a>=0x7ec406 && a<=0x7ec607) || (a>=0x7e9c00 && a<0x7e9f00)) &&
     std::strcmp(name,"hdma-byte")==0)check(smpwide_powerbomb_fetch(context,a));
}
}
