#define NOMINMAX
#include <windows.h>
#include "bridge.h"
#include "../native/registers.hpp"
#include "../../../development/bsnes-hd-gui/libco/libco.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
static void* context;
static cothread_t mainThread;
static std::vector<uint8_t> vram(65536),palette(512),registers(205);
static int result=-1;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(SMPWide::Reg key,unsigned v){auto f=SMPWide::registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)registers[f.offset+i]=v>>(8*i);}
static void rejectFrame(){for(unsigned line=0;line<=224;line++){result=smpwide_raster(context,line,1,{1,vram.data(),vram.size()},{1,palette.data(),palette.size()},{1,registers.data(),registers.size()});if(!result)break;}}
static void coroutine(){rejectFrame();co_switch(mainThread);std::abort();}
int main(int argc,char** argv){
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
  SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e)->LONG{std::fprintf(stderr,"UNHANDLED 0x%08lx\n",e->ExceptionRecord->ExceptionCode);std::fflush(stderr);std::_Exit(7);});
  context=smpwide_create();std::vector<uint8_t> ram(131072),eye(256);
  put(ram,0x998,8);put(ram,0x79b,0xa6e2);put(ram,0x7bb,0xa6ef);put(ram,0x7a5,16);put(ram,0x7a7,16);
  reg(SMPWide::Reg::bgMode,1);reg(SMPWide::Reg::window_bg3_oneEnable,1);
  if(!smpwide_configure_eye(context,eye.data(),eye.size()) || !smpwide_begin(context) || !smpwide_owner(context,{1,ram.data(),ram.size()}) || !smpwide_upload(context,1))return 2;
  if(argc>1 && std::string(argv[1])=="coroutine") {mainThread=co_active();auto thread=co_create(1024*1024,coroutine);co_switch(thread);co_delete(thread);}else rejectFrame();
  std::printf("raster returned %d: %s\n",result,smpwide_error(context));
  if(result && argc>2 && std::string(argv[2])=="fixed") {
    std::vector<uint8_t> native(512*480*3);
    if(smpwide_compose(context,native.data(),native.size(),0) || std::string(smpwide_error(context))!="unqualified eye row")return 3;
    std::puts("PASS: suit-style rejection deferred to host, native fallback can continue");
    if(!smpwide_reset(context) || !smpwide_configure_eye(context,eye.data(),eye.size()))return 4;
    reg(SMPWide::Reg::window_bg3_oneEnable,0);reg(SMPWide::Reg::displayDisable,1);
    if(!smpwide_begin(context) || !smpwide_owner(context,{1,ram.data(),ram.size()}) || !smpwide_upload(context,1))return 5;
    auto thread=co_create(1024*1024,coroutine);co_switch(thread);co_delete(thread);
    if(!result || !smpwide_compose(context,native.data(),native.size(),0))return 6;
    std::puts("PASS: next supported frame composes after fallback/reset");
  }
  smpwide_destroy(context);return 0;
}
