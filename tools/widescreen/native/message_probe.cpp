#include "frame_pipeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x7a5,64);put(ram,0x7a7,64);
  for(unsigned y=0;y<8;y++){vram[y*2]=255;vram[0x2000+y*2]=255;}
  for(unsigned i=0;i<1024;i++)put(vram,0xb000+i*2,0x800);
  put(pal,2,31);put(pal,18,992);
  reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);
  reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,4);
  reg(regs,Reg::bg3_aboveEnable,1);reg(regs,Reg::bg3_priority0,5);
  reg(regs,Reg::bg3_tiledataAddress,0x1000);reg(regs,Reg::bg3_screenAddress,0x5800);
  reg(regs,Reg::hoffset,64);
  auto check=[&](bool message) {
    auto owner=std::make_shared<const OwnerImage>(1,ram);
    SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
    RowTimeline row(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,100);row.finish();
    for(int x=-43;x<299;x++) {
      auto p=row.pixel(x);bool native=x>=0 && x<256;
      auto expected=message && !native?std::array<uint8_t,3>{248,0,0}:std::array<uint8_t,3>{0,248,0};
      if(!p.valid || p.rgb!=expected)throw std::runtime_error("message clipping oracle mismatch");
    }
  };
  ram[0x5b]=0x58;put(ram,0x1c1f,2);check(true);
  ram[0x5b]=0x5a;check(false); // stale message index must not hide normal BG3
  ram[0x5b]=0x58;put(ram,0x1c1f,0);check(false);
  // The message runs during lag frames with no new terrain/OAM upload.
  ram[0x5b]=0x5a;put(ram,0x1c1f,0);FramePipeline pipeline;
  std::vector<uint8_t> table(256),native(512*480*3);
  pipeline.configureEye(table);pipeline.begin();pipeline.owner(1,ram);pipeline.upload(1);
  pipeline.wramWrite(0x1c1f,2,2);pipeline.wramWrite(0x5b,0x58,1);
  auto render=[&](){for(unsigned line=0;line<=224;line++)pipeline.raster(line,1,memoryImage(vram,65536),memoryImage(pal,512),memoryImage(regs,205));pipeline.seal();return pipeline.compose(native,false);};
  auto shown=render();
  for(unsigned x:{0u,341u})if(shown.rgb[(99*342+x)*3]!=248 || shown.rgb[(99*342+x)*3+1])throw std::runtime_error("lag-frame message leaked");
  pipeline.begin();pipeline.wramWrite(0x5b,0x5a,1);auto closed=render();
  for(unsigned x:{0u,341u})if(closed.rgb[(99*342+x)*3] || closed.rgb[(99*342+x)*3+1]!=248)throw std::runtime_error("normal BG3 stayed hidden after message");
  std::cout<<"PASS:1026pixels; live lag-frame open/close and message clipped at both margins, native overlay and ordinary BG3 preserved\n";
}
