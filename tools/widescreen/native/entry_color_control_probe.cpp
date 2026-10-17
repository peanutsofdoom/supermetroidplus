#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>&b,Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=n>>(8*i);}
int main(){try{
 std::vector<uint8_t> ram(131072),v(65536),pal(512),regs(205);ram[0x998]=8;ram[0x7a5]=16;ram[0x7a7]=16;put(regs,Reg::bgMode,1);put(regs,Reg::displayBrightness,15);put(regs,Reg::screen_back_colorEnable,1);put(regs,Reg::screen_colorBlue,31);pal[0]=31;
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 auto make=[&](unsigned line,const auto&r){return RowTimeline(scene,memoryImage(v,65536),memoryImage(pal,512),r,line);};unsigned checks=0;
 for(unsigned address:{0x2130u,0x2131u})for(unsigned blend:{0u,1u})for(unsigned h:{0u,1u,30u,57u}){
  auto after=regs;put(after,address==0x2130?Reg::screen_blendMode:Reg::screen_colorMode,blend);auto row=make(32,regs);row.registerWrite(1,h,address,after);row.finish();auto ref=make(32,after);ref.finish();
  for(int x=-43;x<299;x++){if(row.pixel(x).rgb!=ref.pixel(x).rgb || *row.at(x).registers!=after)throw std::runtime_error("Early CGWSEL phase mismatch");checks++;}
 }
 for(auto scope:{std::pair{32u,58u},std::pair{32u,64u},std::pair{33u,0u}}){bool rejected=false;try{auto row=make(scope.first,regs);row.registerWrite(1,scope.second,0x2130,regs);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("Visible/later CGWSEL incorrectly accepted");}
 auto bad=regs;put(bad,Reg::window_col_aboveMask,3);bool rejected=false;try{auto row=make(32,regs);row.registerWrite(1,0,0x2130,bad);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("Unsupported colour window accepted");
 std::cout<<"PASS "<<checks<<" previsible CGWSEL samples, line/pixel boundaries and unsupported-state guards\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
