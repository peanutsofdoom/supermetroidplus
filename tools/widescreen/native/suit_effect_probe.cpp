#include "mode1.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
struct Outside {Pixel pixel(int,int) const{return {};}};
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{});
  if(rom.size()!=3145728)return 3;
  std::vector<uint8_t> ram(131072),regs(205),table(256),vram(65536),pal(512);
  put(ram,0x79b,0xa6e2);put(ram,0x998,8);EyeState state;state.configure(table);state.configureSuit(Bytes(rom).subspan(0x463c9,128));state.owner(ram);
  auto wr=[&](unsigned a,unsigned v){state.write(a,v,2);put(ram,a,v);};
  reg(regs,Reg::window_bg3_oneEnable,1);reg(regs,Reg::window_col_oneEnable,1);reg(regs,Reg::window_col_belowMask,1);
  reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);reg(regs,Reg::screen_back_colorEnable,1);reg(regs,Reg::screen_colorRed,31);
  std::optional<EyeRow> row;
  if(!state.tryRow(Registers(regs),128,row))throw std::runtime_error("unknown window accepted");
  wr(0x18b4,1);wr(0x18fc,0x88);wr(0x18f0,0xe026);wr(0x1986,0x12);
  auto check=[&](unsigned stage,unsigned positions,unsigned left,unsigned right,int lo,int hi){
    wr(0xdec,stage);wr(0xdee,positions);reg(regs,Reg::window_oneLeft,left);reg(regs,Reg::window_oneRight,right);
    if(state.tryRow(Registers(regs),128,row) || !row || !row->suit)throw std::runtime_error("suit row rejected");
    for(int x=-43;x<299;x++) {
      bool expected=x>=0 && x<256?int(left)<=x && x<=int(right):lo<=x && x<=hi;
      if(row->inside(x)!=expected)throw std::runtime_error("suit window extension differs");
    }
  };
  check(0,0,255,0,255,0);check(1,0x8c64,100,140,100,140);
  check(2,0xe60a,0,254,-14,254);check(2,0xec04,0,255,-20,260);
  check(2,0xff00,0,255,-43,298);check(3,0,0,255,-43,298);
  Outside outside;Mode1<Outside> shader(outside,vram,pal,regs,128,0,0,row);
  for(int x:{-43,-1,256,298})if(shader.pixel(x).rgb!=std::array<uint8_t,3>{248,0,0})throw std::runtime_error("suit light did not extend into room padding");
  check(4,20,255,0,255,0);check(4,20,0,255,-43,298);check(5,0xf708,8,247,8,247);
  Mode1<Outside> dark(outside,vram,pal,regs,128,0,0,row);
  if(dark.pixel(-1).rgb!=std::array<uint8_t,3>{0,0,0})throw std::runtime_error("color math leaked outside suit window");
  check(6,0,255,0,255,0);
  wr(0xdec,7);if(!state.tryRow(Registers(regs),128,row))throw std::runtime_error("invalid suit stage accepted");
  wr(0xdec,0);wr(0x1986,2);wr(0x18b4,0);reg(regs,Reg::window_oneLeft,0);reg(regs,Reg::window_oneRight,0);
  state.owner(ram);if(state.tryRow(Registers(regs),128,row))throw std::runtime_error("suit teardown raster rejected");
  state.owner(ram);if(!state.tryRow(Registers(regs),128,row))throw std::runtime_error("suit teardown history never expired");
  std::cout<<"PASS:3420window positions, suit stages0..6, real curve, padding color math, unknown/invalid rejection\n";
}
