#define NOMINMAX
#include <windows.h>
#include "../native/eye_state.hpp"
#include "../../../development/bsnes-hd-gui/libco/libco.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace SMPWide;
static EyeState eye;static cothread_t parent;
static void writeTable(){eye.write(0x9100,255,2);co_switch(parent);std::abort();}
int main(int argc,char** argv){try {
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});
 if(rom.size()!=3145728)return 3;
 std::vector<uint8_t> ram(131072),regs(205);
 auto put=[&](unsigned a,unsigned v){ram[a]=v;ram[a+1]=v>>8;};
 auto reg=[&](Reg k,unsigned v){auto a=registerFields[unsigned(k)];for(unsigned n=0;n<a.size;n++)regs[a.offset+n]=v>>(8*n);};
 auto check=[](bool value,const char* s){if(!value)throw std::runtime_error(s);};
 put(0x79b,0x9e9f);put(0x7bb,0x9eb1);put(0x998,8);put(0x9000,228);put(0xfba,1032);put(0xfbe,584);put(0x911,883);put(0x915,512);put(0xfee,124);put(0x192c,4);
 eye.configure({rom.data()+0x8c9d4,256});eye.configureXray(rom);eye.owner(ram);
 parent=co_active();auto thread=co_create(1024*1024,writeTable);co_switch(thread);co_delete(thread);
 // At124 +/-4 the left edge is exactly vertical. This was the GPU-03 crash.
 reg(Reg::window_bg3_oneEnable,1);reg(Reg::window_oneLeft,149);unsigned slope=word(rom,0x8c9d4+120*2);unsigned right=std::min(255u,(149*256+slope*(160-72))/256);reg(Reg::window_oneRight,right);
 std::optional<EyeRow> row;check(!eye.tryRow(Registers(regs),160,row) && row && row->trackingEye,"crash-angle row rejected");
 check(!row->inside(-1) && row->inside(149),"vertical edge geometry wrong");
 unsigned sweeps=0;
 for(unsigned angle=0;angle<256;angle++)for(unsigned width=0;width<=4;width++){
  eye.write(0xfee,angle,2);eye.write(0x192c,width,2);auto co=co_create(1024*1024,writeTable);co_switch(co);co_delete(co);sweeps++;
 }
 // Invalid/corrupt parameters still cannot throw on the live callback stack.
 eye.write(0xfee,65535,2);eye.write(0x192c,65535,2);thread=co_create(1024*1024,writeTable);co_switch(thread);co_delete(thread);
 check(eye.tryRow(Registers(regs),160,row)!=nullptr && !row,"unknown angle was silently accepted");
 eye.write(0xfee,84,2);eye.write(0x192c,4,2);eye.write(0x9100,255,2);
 unsigned left=(149*256+word(rom,0x8c9d4+88*2)*88)/256;right=std::min(255,(149*256+word(rom,0x8c9d4+80*2)*88)/256);
 if(left>255){reg(Reg::window_oneLeft,255);reg(Reg::window_oneRight,0);}else{reg(Reg::window_oneLeft,left);reg(Reg::window_oneRight,right);}
 check(!eye.tryRow(Registers(regs),160,row) && row,"supported beam did not recover");
 // Captured native off-screen-origin rows: the upward sweep advances once
 // before writing, unlike the on-screen builder's origin row.
 eye.write(0x911,1051,2);eye.write(0xfba,1032,2);eye.write(0xfee,67,2);eye.write(0x9100,255,2);
 reg(Reg::window_oneLeft,225);reg(Reg::window_oneRight,255);
 check(!eye.tryRow(Registers(regs),67,row) && row && row->trackingEye && row->inside(298) && !row->inside(-1),"off-screen right beam upper phase rejected");
 reg(Reg::window_oneLeft,21);
 check(!eye.tryRow(Registers(regs),72,row) && row && !row->inside(20) && row->inside(21),"off-screen origin-adjacent row rejected");
 eye.write(0xfee,64,2);eye.write(0x192c,0,2);eye.write(0x9100,255,2);
 reg(Reg::window_oneLeft,0);reg(Reg::window_oneRight,255);
 check(!eye.tryRow(Registers(regs),72,row) && row && row->inside(298),"off-screen horizontal startup line shifted");
 eye.write(0x192c,4,2);
 eye.write(0x911,1032,2);eye.write(0xfba,1297,2);eye.write(0xfee,189,2);eye.write(0x9100,255,2);
 reg(Reg::window_oneLeft,0);reg(Reg::window_oneRight,20);
 check(!eye.tryRow(Registers(regs),67,row) && row && row->inside(-43) && !row->inside(298),"off-screen left beam upper phase rejected");
 eye.write(0xfba,1181,2);eye.write(0xfee,67,2);eye.write(0x9100,255,2);
 reg(Reg::window_oneLeft,149);reg(Reg::window_oneRight,255);
 check(!eye.tryRow(Registers(regs),72,row) && row,"on-screen beam convention changed");
 eye.write(0x7bb,0x9ed2,2);check(eye.tryRow(Registers(regs),160,row)!=nullptr,"foreign room state accepted");
 // Captured opposite-facing E6BF scanner in9F64:9F76. Its native table at
 // angle167,width4 contains C8C8 at103,8765 at159 and3D00 at223.
 eye.write(0x79b,0x9f64,2);eye.write(0x7bb,0x9f76,2);
 eye.write(0x911,352,2);eye.write(0x915,512,2);
 eye.write(0xfba,552,2);eye.write(0xfbe,616,2);
 eye.write(0xfee,167,2);eye.write(0x192c,4,2);eye.write(0x9100,255,2);
 reg(Reg::window_oneLeft,200);reg(Reg::window_oneRight,200);
 check(!eye.tryRow(Registers(regs),104,row) && row && row->trackingEye,"Blue Brinstar scanner origin rejected");
 reg(Reg::window_oneLeft,101);reg(Reg::window_oneRight,135);
 check(!eye.tryRow(Registers(regs),160,row) && row && !row->inside(100) && row->inside(101) && row->inside(135) && !row->inside(136),"Blue Brinstar scanner native row changed");
 reg(Reg::window_oneLeft,0);reg(Reg::window_oneRight,61);
 check(!eye.tryRow(Registers(regs),224,row) && row && row->inside(-1) && !row->inside(-43),"Blue Brinstar scanner left margin missing");
 reg(Reg::window_oneRight,62);
 check(eye.tryRow(Registers(regs),224,row)!=nullptr && !row,"Blue Brinstar native witness mismatch accepted");
 reg(Reg::window_oneRight,61);
 eye.write(0x7bb,0x9f90,2);
 check(eye.tryRow(Registers(regs),224,row)!=nullptr,"post-awakening room state accepted as scanner");
 eye.write(0x7bb,0x9f76,2);eye.write(0x79b,0x9f11,2);
 check(eye.tryRow(Registers(regs),224,row)!=nullptr,"unrelated room accepted as scanner");
 std::cout<<"PASS exact crash-angle geometry, "<<sweeps<<" coroutine angle/width sweeps, unknown-angle rejection, recovery and scene guard\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
