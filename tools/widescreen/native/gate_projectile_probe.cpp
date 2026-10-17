#include "enemy_objects.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace SMPWide;
static std::vector<uint8_t> load(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("fixture missing");return {(std::istreambuf_iterator<char>(f)),{}};}
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(int argc,char**argv){try{
 if(argc!=3)return 2;std::filesystem::path root=argv[2];auto rom=load(argv[1]),ram=load(root/"room.wram"),vram=load(root/"room.vram"),pal=load(root/"room.cgram"),regs=load(root/"room.regs");
 std::ifstream f(root/"checkpoint.ppm",std::ios::binary);std::string magic;unsigned w,h,max;f>>magic>>w>>h>>max;f.get();if(magic!="P6"||w!=512||h!=480||max!=255)throw std::runtime_error("native image shape");std::vector<uint8_t> image(w*h*3);f.read((char*)image.data(),image.size());
 unsigned slot=34; if(word(ram,0x1997+slot)!=0xe659)throw std::runtime_error("gate fixture ID");
 int nativeX=int(word(ram,0x1a4b+slot))-int(word(ram,0x911)),top=int(word(ram,0x1a93+slot))-48;EnemySampler native(ram,rom,vram,pal,regs);Terrain ground(ram,vram,pal,Registers(regs).get(Reg::tiledataAddress)*2);unsigned checks=0,opaque=0;
 for(int origin:{-20,272}){
  auto moved=ram;put(moved,0x911,word(ram,0x1a4b+slot)-origin);EnemySampler wide(moved,rom,vram,pal,regs);
  for(unsigned y=0;y<64;y++)for(unsigned x=0;x<16;x++){
   unsigned line=top+y+1;auto p=wide.pixel(origin+x,line);bool visible=native.nativeObjectAt(nativeX+x,line);
   if(bool(p)!=visible)throw std::runtime_error("gate opacity differs from native OAM");
   if(p){unsigned at=((line+7)*2*w+(nativeX+x)*2)*3;for(unsigned c=0;c<3;c++){Registers q(regs);if(q.get(Reg::screen_blendMode)||!q.get(Reg::screen_colorMode)||q.get(Reg::screen_colorHalve))throw std::runtime_error("fixture fixed-color math changed");auto bg=ground.pixel(int(word(ram,0x1a4b+slot))+int(x),int(line)+int(word(ram,0x915)));bool foreground=bg.opaque && q.get(Reg(unsigned(Reg::priority0)+bg.priority))>p->priority;unsigned value=((foreground?bg.color:p->color)>>(c*5))&31;unsigned fixed=q.get(c==0?Reg::screen_colorRed:c==1?Reg::screen_colorGreen:Reg::screen_colorBlue);if(foreground?q.get(Reg::screen_bg1_colorEnable):(q.get(Reg::screen_obj_colorEnable)&&p->palette>=4))value=value>fixed?value-fixed:0;unsigned expected=(value*q.get(Reg::displayBrightness)*16+15)/30;if(image[at+c]!=expected)throw std::runtime_error("gate color differs at "+std::to_string(x)+","+std::to_string(y)+" channel "+std::to_string(c)+" expected "+std::to_string(expected)+" got "+std::to_string(image[at+c]));}opaque++;}
   checks++;
  }
  for(unsigned id:{0u,0xe65au}){put(moved,0x1997+slot,id);if(EnemySampler(moved,rom,vram,pal,regs).pixel(origin,top+1))throw std::runtime_error("gate deletion/scope");}
 }
 std::cout<<"PASS "<<checks<<" actual native PPU/OAM gate pixels at both margins ("<<opaque<<" opaque), deletion and unrelated-ID guard\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
