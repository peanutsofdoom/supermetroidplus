// Power Bomb/liquid line32 register timing and unchanged-map guards.
#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,Reg k,unsigned value){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=value>>(i*8);}
int main(){try{
 std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
 ram[0x998]=8;ram[0x7a5]=16;ram[0x7a7]=16;
 put(regs,Reg::bgMode,1);put(regs,Reg::displayBrightness,15);put(regs,Reg::window_twoRight,255);
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene;scene.view=owner;scene.assets=owner;scene.kind=SceneKind::Ordinary;
 auto make=[&](unsigned line,bool pb,bool startup=false){auto selected=scene;if(startup){auto active=ram;active[0x593]=0x80;selected.view=std::make_shared<const OwnerImage>(2,active);}std::optional<EyeRow> effect;if(pb)effect=EyeRow{{},int(line),0,255,false,-43,298,true};return RowTimeline(selected,memoryImage(vram,65536),memoryImage(pal,512),regs,line,effect);};
 for(unsigned address:{0x212cu,0x212du}){
  auto after=regs;Reg field=address==0x212c?Reg::aboveEnable:Reg::belowEnable;put(after,field,1);
  auto row=make(32,true);row.registerWrite(1,68,address,after);row.finish();
  for(int x:{-43,0,2})if(Registers(*row.at(x).registers).get(field)!=0)throw std::runtime_error("enable applied before pixel3");
  for(int x:{3,255,298})if(Registers(*row.at(x).registers).get(field)!=1)throw std::runtime_error("enable missing after pixel3");
  for(unsigned h:{68u,78u,82u}){
   auto startup=make(32,false,true);startup.registerWrite(1,h,address,after);startup.finish();int split=(h-58+3)/4;
   for(int x:{-43,0,split-1})if(Registers(*startup.at(x).registers).get(field)!=0)throw std::runtime_error("startup enable before native boundary");
   for(int x:{split,255,298})if(Registers(*startup.at(x).registers).get(field)!=1)throw std::runtime_error("startup enable missing at native boundary");
   bool rejected=false;try{auto bad=make(33,false,true);bad.registerWrite(1,h,address,after);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("startup accepted outside line32");
  }
  for(auto scope:{std::pair{32u,false},std::pair{33u,true}}){bool rejected=false;try{auto bad=make(scope.first,scope.second);bad.registerWrite(1,68,address,after);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("late enable scope widened");}
 }
 for(unsigned h:{0u,34u,100u}){
  auto row=make(32,false);row.registerWrite(1,h,0x2109,regs);row.finish();
  if(*row.at(-43).registers!=regs || *row.at(298).registers!=regs)throw std::runtime_error("inert map changed state");
  auto changed=regs;put(changed,Reg::bg3_screenAddress,0x5800);bool rejected=false;
  try{auto bad=make(32,false);bad.registerWrite(1,h,0x2109,changed);}catch(const std::invalid_argument&){rejected=true;}
  if(!rejected)throw std::runtime_error("unqualified changed map accepted");
 }
 for(unsigned h:{0u,20u,31u}){
  auto changed=regs;put(changed,Reg::screen_bg1_colorEnable,1);
  auto row=make(32,true);row.registerWrite(1,h,0x2131,changed);row.finish();
  for(int x:{-43,0,255,298})if(!Registers(*row.at(x).registers).get(Reg::screen_bg1_colorEnable))throw std::runtime_error("previsible PB math reset missing");
  auto ordinary=make(32,false);ordinary.registerWrite(1,h,0x2131,changed);ordinary.finish();
  if(!Registers(*ordinary.at(0).registers).get(Reg::screen_bg1_colorEnable))throw std::runtime_error("previsible ordinary math reset missing");
 }
 std::cout<<"PASS late PB layer enables at pixel3, both margins, scope guards and inert BG3 writes\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
