// SPDX-License-Identifier: GPL-3.0-or-later
#include <sfc/sfc.hpp>
#undef platform
#include "recorder.h"
#include "input_stream.hpp"
#include "sha256.hpp"
#include "ready_start.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <iomanip>
#include <typeinfo>
#include <iostream>
#include <cstdlib>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif
namespace SF=SuperFamicom;
namespace fs=std::filesystem;
using SMPPlaytest::Digest;
using SMPPlaytest::sha256;
namespace {
bool wanted=false,replay=false,comparison=false,active=false,finished=false,failed=false,prepared=false;
bool readyRequested=false,readyProfile=false,handedOff=false,explorationBudget=false;
unsigned readyStableFrames=0;
uint64_t startupFrames=0,startupCycles=0;
std::string startupStateHash;
unsigned frameLimit=7200;
uint64_t frames=0,videoFrames=0,auditEvents=0,audioFrames=0,markers=0;
uint64_t expectedFrames=0,expectedCycles=0;
fs::path root,resultRoot,originalExecutable;
std::vector<uint8_t> initialSram;
std::ofstream inputOut,frameOut,markerOut;
std::ifstream inputIn,frameIn;
std::unique_ptr<SMPPlaytest::InputStream> inputs;
Digest audit,audio;
std::string videoHash,status,config,frontend,exeHash,recordedExeHash,romHash,initialState;
std::vector<double> workMilliseconds,frameMilliseconds;
std::chrono::steady_clock::time_point start,frameStart;
std::chrono::steady_clock::time_point lastBudgetCheck;
double observedMilliseconds=0;

SMPPlaytest::ReadyState readyState() {
  auto word=[](unsigned a){return unsigned(SF::cpu.wram[a])|(unsigned(SF::cpu.wram[a+1])<<8);};
  return {word(0x998),word(0x79b),word(0x7bb),word(0xa42),word(0xa44),word(0xaf6),word(0xafa),
          word(0x9c2),word(0x9c4),word(0x9a4),word(0x9c6),word(0x9c8),SF::cpu.wram[0xa1f],word(0xb2e),word(0xb2c)};
}

std::vector<uint8_t> read(const fs::path& p) {
  std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read "+p.filename().string());
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
std::string fileHash(const fs::path& p) {
  std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot hash file");
  Digest d;char block[65536];while(f){f.read(block,sizeof(block));d.add(block,size_t(f.gcount()));}
  if(!f.eof())throw std::runtime_error("File hash read failed");return d.hex();
}
void write(const fs::path& p,const void* data,size_t bytes) {
  std::ofstream f(p,std::ios::binary);f.write(static_cast<const char*>(data),bytes);f.close();
  if(!f)throw std::runtime_error("Cannot write "+p.filename().string());
}
void write(const fs::path& p,const std::string& text){write(p,text.data(),text.size());}
void diskCheck() {
  if(fs::space(root).available<20ull*1024*1024*1024)throw std::runtime_error("20 GiB free-space floor reached");
  uint64_t bytes=0;
  for(const auto& entry:fs::directory_iterator(resultRoot))if(entry.is_regular_file())bytes+=entry.file_size();
  if(bytes>256ull*1024*1024)throw std::runtime_error("256 MiB trial output budget exceeded");
}
template<class F>void guarded(F action) {
  if(!active)return;
  try{action();}catch(const std::exception& e){smpr_abort(e.what());}
}
void event(uint64_t tag,std::initializer_list<uint64_t> words) {
  audit.word(tag);audit.word(SMPTrace::cycles);for(auto v:words)audit.word(v);auditEvents++;
}
template<class F>std::string snapshot(F serialize) {
  nall::serializer size;serialize(size);nall::serializer data(size.size());serialize(data);
  return sha256(data.data(),data.size());
}
std::string state() {
  auto group=[](const char* name,auto f){return std::string("\"")+name+"\":\""+snapshot(f)+"\"";};
  return "{"+group("wram",[](auto& s){s.array(SF::cpu.wram);})+","+
    group("sram",[](auto& s){s.array(SF::cartridge.ram.data(),SF::cartridge.ram.size());})+","+
    group("cpu",[](auto& s){SF::cpu.serialize(s);})+","+
    group("ppu",[](auto& s){SF::ppu.serialize(s);})+","+
    group("apu",[](auto& s){SF::smp.serialize(s);SF::dsp.serialize(s);})+","+
    group("controllers",[](auto& s){for(auto p:{&SF::controllerPort1,&SF::controllerPort2}) {
      s.integer(p->port);auto pad=dynamic_cast<SF::Gamepad*>(p->device);
      unsigned type=pad?1:0;s.integer(type);
      if(pad)pad->diagnosticSerialize(s);
      else if(p->device && typeid(*p->device)!=typeid(SF::Controller))throw std::runtime_error("Only gamepad/empty ports supported");
    }})+","+group("scheduler",[](auto& s){
      unsigned mode=unsigned(SF::scheduler.mode),e=unsigned(SF::scheduler.event);
      unsigned thread=SF::scheduler.active==SF::cpu.thread?1:SF::scheduler.active==SF::smp.thread?2:SF::scheduler.active==SF::ppu.thread?3:0;
      s.integer(mode);s.integer(e);s.integer(thread);s.boolean(SF::scheduler.desynchronized);SF::random.serialize(s);
    })+"}";
}
void verifyLine(const std::string& line) {
  if(replay) {
    char buffer[4096];frameIn.getline(buffer,sizeof(buffer));std::string expected=buffer;
    // Windows text streams wrote CRLF; Linux getline removes only LF. Compare
    // identical witness content without weakening any guest/output hash check.
    if(!expected.empty() && expected.back()=='\r')expected.pop_back();
    if(!frameIn || expected!=line) {
      write(resultRoot/"first-divergence.txt","Frame "+std::to_string(frames)+"\nEXPECTED\n"+expected+"\nACTUAL\n"+line+"\n");
      throw std::runtime_error("Guest/output witness differs at frame "+std::to_string(frames));
    }
  }
  frameOut<<line<<'\n';if(!frameOut)throw std::runtime_error("Frame witness write failed");
}
std::string field(const nall::Markup::Node& document,const char* name) {return (const char*)document[name].text();}
[[noreturn]] void callbackFailure(const std::string& message) {
  smpr_abort(message.c_str());
  std::cerr<<status<<std::endl;
  // Do not unwind libco stacks or resume the guest with substituted input.
  // This exits only this owned diagnostic process, after flushing evidence.
#ifdef _WIN32
  TerminateProcess(GetCurrentProcess(),2);
#endif
  std::_Exit(2);
}
}
void smpr_request(const char* directory,bool play) {
  if(wanted)throw std::runtime_error("Only one recording/replay per launch");
  wanted=true;replay=play;root=fs::absolute(fs::u8path(directory));
  if(replay) {
    if(fs::file_size(root/"initial-sram.bin")!=8192)throw std::runtime_error("Invalid initial SRAM length");
    initialSram=read(root/"initial-sram.bin");
  }
}
void smpr_request_compare(const char* directory,const char* original) {
  if(!original || !*original)throw std::runtime_error("Comparison requires the original executable path");
  smpr_request(directory,true);
  comparison=true;originalExecutable=fs::absolute(fs::u8path(original));
}
std::string smpr_diagnostic_state(){return state();}
void smpr_ready_ship(bool exploration){if(!wanted || prepared || replay || comparison)throw std::runtime_error("Ready start requires a new recording");readyRequested=true;explorationBudget=exploration;if(exploration)frameLimit=SMPPlaytest::ExplorationFrameLimit;}
bool smpr_ready_for_human(){return active && readyProfile && handedOff && !replay;}
uint64_t smpr_human_frames(){return readyProfile && handedOff?frames-startupFrames:0;}
void smpr_limit(unsigned n){if(!n || n>(explorationBudget?SMPPlaytest::ExplorationFrameLimit:7200))throw std::runtime_error("Trial limit exceeds its explicit recording budget");frameLimit=n;}
bool smpr_requested(){return wanted;}
bool smpr_active(){return active;}
bool smpr_replaying(){return active && replay;}
bool smpr_readonly_replay(){return wanted && replay;}
bool smpr_finished(){return wanted && finished;}
const char* smpr_status(){return status.c_str();}
const uint8_t* smpr_initial_sram(size_t* size) {
  if(!wanted || !replay)return nullptr;*size=initialSram.size();return initialSram.data();
}
void smpr_before_power(const uint8_t* rom,size_t size,bool compatible) {
  if(!wanted)return;
  try {
    if(prepared)throw std::runtime_error("Reset/reload requires a new recording launch");
    if(fs::space(root.parent_path()).available<20ull*1024*1024*1024+256ull*1024*1024)
      throw std::runtime_error("Trial needs 256 MiB reserved above the 20 GiB free-space floor");
    if(!compatible)throw std::runtime_error("Incompatible frontend options for recording");
    if(std::string((const char*)SF::configuration.hacks.entropy)!="None")throw std::runtime_error("Recording requires declared Entropy=None");
    auto& h=SF::configuration.hacks;
    if(h.hotfixes || h.cpu.overclock!=100 || h.cpu.fastMath || h.cpu.fastJoypadPolling ||
       h.ppu.fast || h.ppu.deinterlace || h.ppu.noSpriteLimit || h.ppu.noVRAMBlocking ||
       h.ppu.mode7.scale!=1 || h.ppu.mode7.widescreen || h.dsp.fast || h.dsp.cubic ||
       h.dsp.echoShadow || h.coprocessor.delayedSync || h.coprocessor.preferHLE)
      throw std::runtime_error("Trial requires the native guest configuration");
    romHash=sha256(rom,size);
    if(romHash!="12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72")throw std::runtime_error("Unsupported ROM digest");
    if(SF::cartridge.ram.size()!=8192)throw std::runtime_error("Unexpected SRAM geometry");
    std::vector<uint8_t> loaded(SF::cartridge.ram.data(),SF::cartridge.ram.data()+8192);
    if(replay && loaded!=initialSram)throw std::runtime_error("Cartridge loader did not preserve initial SRAM");
    initialSram=loaded;
#ifdef _WIN32
    wchar_t executable[32768];auto length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length || length==32768)throw std::runtime_error("Cannot identify executable");
    exeHash=fileHash(fs::path(executable));
#else
    exeHash=fileHash(fs::read_symlink("/proc/self/exe"));
#endif
    if(replay) {
      for(auto name:{"complete.bml","configuration.bml","frontend.txt"})
        if(fs::file_size(root/name)>65536)throw std::runtime_error("Oversized bundle metadata");
      if(fs::file_size(root/"inputs.bin")>64ull*1024*1024 || fs::file_size(root/"frames.jsonl")>16ull*1024*1024)
        throw std::runtime_error("Bundle exceeds bounded input/witness size");
      auto raw=read(root/"complete.bml");std::string text(raw.begin(),raw.end());auto doc=nall::BML::unserialize(text.c_str());
      if(field(doc,"format")!="smp-playtest-v1" || field(doc,"status")!="recorded")throw std::runtime_error("Incomplete recording");
      recordedExeHash=field(doc,"executable-sha256");
      if(comparison) {
        if(fileHash(originalExecutable)!=recordedExeHash)throw std::runtime_error("Supplied original executable differs from recorded build identity");
        if(exeHash==recordedExeHash)throw std::runtime_error("Comparison requires a different candidate executable");
      } else if(recordedExeHash!=exeHash)throw std::runtime_error("Build/ROM identity differs");
      if(field(doc,"rom-sha256")!=romHash)throw std::runtime_error("Build/ROM identity differs");
      if(field(doc,"input-sha256")!=fileHash(root/"inputs.bin") || field(doc,"frames-sha256")!=fileHash(root/"frames.jsonl") ||
         field(doc,"initial-sram-sha256")!=sha256(initialSram.data(),initialSram.size()))throw std::runtime_error("Bundle digest differs");
      expectedFrames=std::stoull(field(doc,"frames"));expectedCycles=std::stoull(field(doc,"cycles"));
      if(readyRequested)throw std::runtime_error("Ready-start option cannot alter replay");
      auto profile=field(doc,"ready-profile");
      readyProfile=!profile.empty();
      auto budget=field(doc,"recording-budget");
      if(!budget.empty() && (budget!=SMPPlaytest::ExplorationBudget || !readyProfile))throw std::runtime_error("Unknown or incompatible recording budget");
      explorationBudget=!budget.empty();
      if(readyProfile) {
        if(profile!=SMPPlaytest::ReadyProfile || fs::file_size(root/"ready-start.bml")>65536 ||
           fileHash(root/"ready-start.bml")!=field(doc,"ready-start-sha256"))throw std::runtime_error("Ready-start provenance differs");
        auto bytes=read(root/"ready-start.bml");std::string source(bytes.begin(),bytes.end());auto ready=nall::BML::unserialize(source.c_str());
        if(field(ready,"format")!="smp-ready-start-v1" || field(ready,"profile")!=SMPPlaytest::ReadyProfile)
          throw std::runtime_error("Unknown ready-start profile");
        startupFrames=std::stoull(field(ready,"startup-frames"));startupCycles=std::stoull(field(ready,"startup-cycles"));
        startupStateHash=field(ready,"state-sha256");auto humanLimit=std::stoull(field(doc,"human-frame-limit"));
        if(!humanLimit || humanLimit>(explorationBudget?SMPPlaytest::ExplorationFrameLimit:7200))throw std::runtime_error("Invalid human frame limit");frameLimit=unsigned(humanLimit);
        if(!startupFrames || startupFrames>SMPPlaytest::StartupFrameLimit || !startupCycles || startupCycles>expectedCycles ||
           startupStateHash.size()!=64 || startupStateHash.find_first_not_of("0123456789abcdef")!=std::string::npos ||
           field(ready,"initial-sram-sha256")!=SMPPlaytest::ReadySramSha ||
           startupFrames>expectedFrames || expectedFrames-startupFrames>frameLimit ||
           std::stoull(field(doc,"human-frames"))!=expectedFrames-startupFrames ||
           sha256(initialSram.data(),initialSram.size())!=SMPPlaytest::ReadySramSha)
          throw std::runtime_error("Invalid ready-start bounds/checkpoint");
      } else if(fs::exists(root/"ready-start.bml"))throw std::runtime_error("Undeclared ready-start metadata");
      if(!expectedFrames || expectedFrames>(readyProfile?SMPPlaytest::StartupFrameLimit+(explorationBudget?SMPPlaytest::ExplorationFrameLimit:7200):7200))throw std::runtime_error("Invalid recorded frame bound");
#ifdef _WIN32
      auto id=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64());
#else
      auto id=std::to_string(getpid())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
      resultRoot=root/(comparison?"cross-build-comparisons":"replays")/id;fs::create_directories(resultRoot.parent_path());
      if(!fs::create_directory(resultRoot))throw std::runtime_error("Replay output exists");
      inputIn.open(root/"inputs.bin",std::ios::binary);frameIn.open(root/"frames.jsonl");
      inputs=std::make_unique<SMPPlaytest::InputStream>(static_cast<std::istream&>(inputIn));
    } else {
      if(!fs::create_directory(root))throw std::runtime_error("Recording folder already exists");
      resultRoot=root;write(root/"initial-sram.bin",initialSram.data(),initialSram.size());
      inputOut.open(root/"inputs.bin",std::ios::binary);
      inputs=std::make_unique<SMPPlaytest::InputStream>(static_cast<std::ostream&>(inputOut));
      markerOut.open(root/"markers.jsonl");
      readyProfile=readyRequested;
      if(readyProfile && sha256(initialSram.data(),initialSram.size())!=SMPPlaytest::ReadySramSha)
        throw std::runtime_error("Ready start requires the packaged verified ship save");
    }
    inputs->onFailure(callbackFailure);
    diskCheck();frameOut.open(resultRoot/"frames.jsonl");if(!frameOut)throw std::runtime_error("Cannot open frame witness");
    SMPTrace::cycles=0;prepared=true;status="Preparing explicit reset-based trial";
  } catch(const std::exception& e){smpr_abort(e.what());}
}
void smpr_begin(const char* configuration,const char* front) {
  if(!wanted || failed)return;
  try {
    if(!prepared)throw std::runtime_error("No verified power/reset context");
    if(sha256(SF::cartridge.ram.data(),SF::cartridge.ram.size())!=sha256(initialSram.data(),initialSram.size()))throw std::runtime_error("Power changed initial SRAM");
    SF::random.seed(0,0); // Same established M1 reset profile, in both modes.
    config=configuration;frontend=front;
    if(replay) {
      auto expected=read(root/"configuration.bml");
      if(std::string(expected.begin(),expected.end())!=config)throw std::runtime_error("Core configuration differs");
      expected=read(root/"frontend.txt");
      const std::string expectedFrontend(expected.begin(),expected.end());
      // Explicit cross-build comparison may consume the activation variant's
      // headless 48kHz baseline. Ordinary replay still requires exact frontend
      // identity; every input/audio/native/state witness is checked below.
      const bool headlessComparison=comparison &&
        expectedFrontend=="diagnostic CPU-side renderer;audio=48000;mode=off;no GUI presentation" &&
        frontend=="audio-frequency=48000;wide=true;tv=true;native-output=uint32-le-visible-rows";
      if(expectedFrontend!=frontend && !headlessComparison)throw std::runtime_error("Audio/presentation configuration differs");
      if(headlessComparison)write(resultRoot/"cross-frontend-comparison.txt",expectedFrontend+"\nTO\n"+frontend+"\nAll guest/audio/native witnesses remain mandatory.\n");
    } else {write(root/"configuration.bml",config);write(root/"frontend.txt",frontend);}
    frames=videoFrames=auditEvents=audioFrames=0;audit.reset();audio.reset();
    initialState=state();verifyLine("{\"initial\":"+initialState+"}");
    active=true;start=lastBudgetCheck=std::chrono::steady_clock::now();status=comparison?"Comparing recorded build":replay?"Verifying replay":"Recording - Ctrl+Shift+F9 marks a bug; Ctrl+Shift+F10 stops";
    if(readyProfile && !replay)status="Loading ship checkpoint - please wait";
  } catch(const std::exception& e){smpr_abort(e.what());}
}
bool smpr_before_frame(const char* configuration,bool compatible) {
  if(!wanted)return true;if(finished || failed)return false;
  guarded([&]{if(!compatible || config!=configuration)throw std::runtime_error("Configuration/run mode changed during trial");
    if(frames%60==0)diskCheck();observedMilliseconds=0;frameStart=std::chrono::steady_clock::now();});
  return active;
}
void smpr_tick() {
  if(active && std::chrono::steady_clock::now()-lastBudgetCheck>=std::chrono::seconds(5))
    guarded([&]{diskCheck();lastBudgetCheck=std::chrono::steady_clock::now();});
}
void smpr_after_frame() {
  guarded([&]{
    auto begin=std::chrono::steady_clock::now();auto cycle=SMPTrace::cycles;
    if(videoFrames!=frames+1)throw std::runtime_error("Native frame callback count differs");frames++;
    event(7,{frames});
    auto currentState=state();
    std::ostringstream line;line<<"{\"frame\":"<<frames<<",\"cycle\":"<<cycle<<",\"events\":"<<auditEvents
      <<",\"execution\":\""<<audit.hex()<<"\",\"video\":\""<<videoHash<<"\",\"audio_frames\":"<<audioFrames
      <<",\"audio\":\""<<audio.hex()<<"\",\"state\":"<<currentState<<"}";
    if(cycle!=SMPTrace::cycles)throw std::runtime_error("Witness capture advanced guest cycles");verifyLine(line.str());
    if(readyProfile && !handedOff) {
      bool valid=SMPPlaytest::readyState(readyState());
      if(replay) {
        if(frames==startupFrames) {
          if(!valid || cycle!=startupCycles || sha256(currentState.data(),currentState.size())!=startupStateHash)
            throw std::runtime_error("Ready handoff state differs");
          auto metadata=read(root/"ready-start.bml");write(resultRoot/"ready-start.bml",metadata.data(),metadata.size());handedOff=true;
        }
      } else {
        readyStableFrames=valid?readyStableFrames+1:0;
        if(readyStableFrames>=8) {
          startupFrames=frames;startupCycles=cycle;startupStateHash=sha256(currentState.data(),currentState.size());handedOff=true;
          std::ostringstream meta;meta<<std::setprecision(12)<<"format: smp-ready-start-v1\nprofile: "<<SMPPlaytest::ReadyProfile
            <<"\nstartup-frames: "<<frames<<"\nstartup-cycles: "<<cycle<<"\nstate-sha256: "<<startupStateHash
            <<"\ninitial-sram-sha256: "<<sha256(initialSram.data(),initialSram.size())
            <<"\nwall-seconds-to-ready: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
          write(root/"ready-start.bml",meta.str());status="READY TO PLAY - F9 marker / F10 stop with Ctrl+Shift";
        } else if(frames>=SMPPlaytest::StartupFrameLimit)throw std::runtime_error("Checkpoint loading did not reach verified gameplay in its bounded startup");
      }
    }
    observedMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    workMilliseconds.push_back(observedMilliseconds);
    frameMilliseconds.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());
  });
  if(active && (replay?frames>=expectedFrames:readyProfile?(handedOff && frames-startupFrames>=frameLimit):frames>=frameLimit))smpr_stop();
}
void smpr_stop() {
  if(!active)return;
  try {
    if(!frames)throw std::runtime_error("Empty trial");
    if(readyProfile && !handedOff)throw std::runtime_error("Stopped before verified gameplay handoff; startup evidence preserved");
    diskCheck();
    if(replay && (frames!=expectedFrames || SMPTrace::cycles!=expectedCycles))throw std::runtime_error("Replay stopped before recorded endpoint");
    inputs->finish(SMPTrace::cycles,frames);
    if(replay && frameIn.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unconsumed frame witnesses");
    frameOut.flush();frameOut.close();if(!frameOut)throw std::runtime_error("Frame witness close failed");
    if(inputOut.is_open()){inputOut.close();if(!inputOut)throw std::runtime_error("Input close failed");}markerOut.close();
    auto percentile=[](std::vector<double> v,double q){std::sort(v.begin(),v.end());return v[size_t((v.size()-1)*q)];};
    std::ostringstream result;result<<std::setprecision(12)<<"format: "<<(comparison?"smp-cross-build-comparison-v1":"smp-playtest-v1")
      <<"\nstatus: "<<(comparison?"comparison-verified":replay?"verified":"recorded")
      <<"\nframes: "<<frames<<"\ncycles: "<<SMPTrace::cycles<<"\nevents: "<<auditEvents
      <<"\nlatches: "<<inputs->latchCount()<<"\npolls: "<<inputs->pollCount()<<"\ninput-bytes: "<<inputs->byteCount()
      <<"\nexecutable-sha256: "<<exeHash;
    if(comparison)result<<"\nrecorded-executable-sha256: "<<recordedExeHash<<"\ncandidate-executable-sha256: "<<exeHash;
    if(explorationBudget)result<<"\nrecording-budget: "<<SMPPlaytest::ExplorationBudget;
    if(readyProfile)result<<"\nready-profile: "<<SMPPlaytest::ReadyProfile<<"\nready-start-sha256: "<<fileHash(root/"ready-start.bml")
      <<"\nhuman-frame-limit: "<<frameLimit<<"\nhuman-frames: "<<frames-startupFrames;
    result<<"\nrom-sha256: "<<romHash<<"\ninitial-sram-sha256: "<<sha256(initialSram.data(),initialSram.size())
      <<"\ninput-sha256: "<<fileHash(root/"inputs.bin")<<"\nframes-sha256: "<<fileHash(resultRoot/"frames.jsonl")
      <<"\nmeasured-callback-work-median-ms: "<<percentile(workMilliseconds,.5)<<"\nmeasured-callback-work-p95-ms: "<<percentile(workMilliseconds,.95)
      <<"\nmain-iteration-median-ms: "<<percentile(frameMilliseconds,.5)<<"\nmain-iteration-p95-ms: "<<percentile(frameMilliseconds,.95)
      <<"\nmeasurement-scope: video and end-frame witness callbacks; execution-audit overhead requires paired benchmark\n";
    write(resultRoot/"complete.tmp",result.str());fs::rename(resultRoot/"complete.tmp",resultRoot/"complete.bml");
    active=false;finished=true;status=comparison?"Cross-build comparison verified":replay?"Replay verified":"Recording saved - close this window before replay";
  }catch(const std::exception& e){smpr_abort(e.what());}
}
void smpr_abort(const char* why) {
  if(!wanted || failed || (finished && !active))return;failed=true;active=false;finished=true;status=std::string("PLAYTEST FAILED: ")+why;
  if(!resultRoot.empty()) {
    auto text=status+"\nframe="+std::to_string(frames)+"\ncycle="+std::to_string(SMPTrace::cycles)+"\n";
#ifdef _WIN32
    HANDLE file=CreateFileW((resultRoot/"failure.txt").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr);FlushFileBuffers(file);CloseHandle(file);}
#else
    // Evidence survives the immediate _Exit used for failed coroutine callbacks.
    int file=::open((resultRoot/"failure.txt").c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(file>=0) {
      size_t at=0;
      while(at<text.size()) {
        auto n=::write(file,text.data()+at,text.size()-at);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        at+=size_t(n);
      }
      ::fsync(file);::close(file);
    }
#endif
  }
  if(frameOut.is_open())frameOut.flush();if(inputOut.is_open())inputOut.flush();
}
void smpr_marker(){guarded([&]{if(replay)return;diskCheck();markerOut<<"{\"marker\":"<<++markers<<",\"frame\":"<<frames<<",\"cycle\":"<<SMPTrace::cycles<<",\"input_event\":"<<inputs->eventCount()<<"}\n";markerOut.flush();if(!markerOut)throw std::runtime_error("Marker write failed");status=(readyProfile&&!handedOff?"Loading checkpoint - marker ":"Recording - bug marker ")+std::to_string(markers)+" saved";});}
int16_t smpr_poll(unsigned p,unsigned d,unsigned i,int16_t value) {
  guarded([&]{if(readyProfile && !replay && !handedOff)value=SMPPlaytest::startupPoll(p,i,SMPPlaytest::startupButtons(frames,readyState().game));
    value=inputs->poll(SMPTrace::cycles,videoFrames,p,d,i,value);event(8,{p,d,i,uint16_t(value)});});return value;
}
void smpr_latch(unsigned p){guarded([&]{inputs->latch(SMPTrace::cycles,videoFrames,p);event(3,{inputs->latchCount()-1,p});});}
void smpr_bus(unsigned a,unsigned v,bool w){guarded([&]{event(1,{a,v,w});});}
void smpr_write(const char* s,unsigned a,unsigned v,unsigned width){guarded([&]{unsigned id=!std::strcmp(s,"wram")?1:!std::strcmp(s,"vram")?2:!std::strcmp(s,"oam")?3:!std::strcmp(s,"cgram")?4:!std::strcmp(s,"register")?5:0;if(!id)throw std::runtime_error("Unknown committed memory space");event(2,{id,a,v,width});});}
void smpr_interrupt(const char* k,const char* p){guarded([&]{event(k[0]=='n'?5:6,{p[0]=='b'?0ull:1ull});});}
void smpr_dma(unsigned c,const char* p){guarded([&]{event(4,{c,p[0]=='b'?0ull:1ull});});}
void smpr_signal(const char* name,unsigned channel,unsigned a,unsigned b){guarded([&]{
  static const char* names[]={"nmi-line","irq-line","hdma-reload-line","hdma-transfer-begin","hdma-byte","hdma-transfer-end","hdma-advance"};
  unsigned id=0;for(unsigned i=0;i<7;i++)if(!std::strcmp(name,names[i]))id=i+1;
  if(!id)throw std::runtime_error("Unknown native signal");event(9,{id,channel,a,b});
});}
void smpr_video(const uint32_t* p,unsigned pitch,unsigned w,unsigned h,unsigned scale){guarded([&]{auto begin=std::chrono::steady_clock::now();Digest d;d.word(w);d.word(h);d.word(scale);for(unsigned y=0;y<h;y++)d.add(reinterpret_cast<const uint8_t*>(p)+y*pitch,w*4);videoHash=d.hex();videoFrames++;observedMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();});}
void smpr_audio(const double* samples,unsigned channels){guarded([&]{audio.word(channels);audio.add(samples,channels*sizeof(double));audioFrames++;});}
