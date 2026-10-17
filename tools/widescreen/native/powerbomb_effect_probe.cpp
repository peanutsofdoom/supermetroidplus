#include "mode1.hpp"
#include <fstream>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
struct Ground {Pixel pixel(int,int)const{return {31,0,true,true};}};
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{}),ram(131072),table(256),regs(205),vram(65536),pal(512);
 EyeState e;e.configure(table);e.configurePowerBomb(rom);put(ram,0x79b,0xa6e2);put(ram,0x592,0x8000);put(ram,0x18b4,1);put(ram,0x18fc,0x88);put(ram,0x18f0,0x8de9);put(ram,0xcea,0x4000);put(ram,0xce6,266);put(ram,0xce8,411);e.owner(ram);e.write(0xc406,0,1);
 reg(regs,Reg::window_bg3_twoEnable,1);reg(regs,Reg::window_twoLeft,0);reg(regs,Reg::window_twoRight,73);
 std::optional<EyeRow> row;if(auto error=e.tryRow(Registers(regs),100,row))throw std::runtime_error(error);
 if(!row || !row->powerBomb || !row->inside(-20) || row->inside(-54))throw std::runtime_error("unclamped shape");
 e.write(0xc407,0,1);e.write(0xc507,73,1);e.powerBombFetch(0x7ec407);e.powerBombFetch(0x7ec507);
 e.write(0xce6,276,2);e.write(0xc408,0,1);e.write(0xc508,83,1);
 if(auto error=e.tryRow(Registers(regs),100,row))throw std::runtime_error("unfetched table write changed active window");
 reg(regs,Reg::window_twoRight,72);if(!e.tryRow(Registers(regs),100,row))throw std::runtime_error("bad native bounds accepted");reg(regs,Reg::window_twoRight,73);e.tryRow(Registers(regs),100,row);
 reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,4);
 reg(regs,Reg::bg3_belowEnable,1);reg(regs,Reg::bg3_priority0,5);reg(regs,Reg::bg3_tiledataAddress,0x1000);reg(regs,Reg::bg3_screenAddress,0x5800);reg(regs,Reg::window_bg3_belowEnable,1);
 reg(regs,Reg::screen_blendMode,1);reg(regs,Reg::screen_bg1_colorEnable,1);reg(regs,Reg::screen_colorGreen,31);
 for(unsigned i=0;i<1024;i++)put(vram,0xb000+2*i,0x800);for(unsigned y=0;y<8;y++)vram[0x2000+y*2]=255;
 Ground ground;Mode1<Ground> renderer(ground,vram,pal,regs,100,0,0,row);
 if(renderer.pixel(-20).rgb!=std::array<uint8_t,3>{248,248,0} || renderer.pixel(-60).rgb!=std::array<uint8_t,3>{248,0,0})throw std::runtime_error("Power Bomb subscreen math");
 std::cout<<"PASS native-scaled window reconstruction, negative margin, mismatch rejection and PB subscreen color math\n";
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
