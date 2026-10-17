#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>& b,Reg key,unsigned v){auto f=registerFields[unsigned(key)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){
  std::vector<uint8_t> ram(131072),vram(65536),pal(512),regs(205);
  put(ram,0x998,8);put(ram,0x79b,0xa4da);put(ram,0x7bb,0xa4e7);
  put(ram,0x7a5,64);put(ram,0x7a7,128);put(ram,0x921,512);put(ram,0x917,411);
  for(unsigned q=0;q<4;q++){put(ram,0xa008+q*2,1);put(ram,0xa010+q*2,2);}
  for(unsigned y=0;y<128;y++)for(unsigned x=0;x<64;x++)put(ram,0x19602+(y*64+x)*2,x<40?1:2);
  for(unsigned y=0;y<8;y++){vram[32+y*2]=255;vram[64+y*2+1]=255;vram[96+y*2]=vram[96+y*2+1]=255;}
  put(pal,2,992);put(pal,4,31);put(pal,6,31744);
  reg(regs,Reg::bgMode,1);reg(regs,Reg::displayBrightness,15);
  reg(regs,Reg::aboveEnable,1);reg(regs,Reg::priority0,6);reg(regs,Reg::hoffset,549);
  reg(regs,Reg::bg2_aboveEnable,1);reg(regs,Reg::bg2_priority0,4);
  reg(regs,Reg::bg2_screenAddress,0x4800);reg(regs,Reg::bg2_hoffset,411+512);
  // Actual photo camera549 => native 3/4 parallax camera411. Leave the VRAM
  // background ring deliberately wrong (blue), then simulate changed residency.
  for(auto sceneId:{std::pair{0xd104u,0xd111u},std::pair{0xd95eu,0xd970u},std::pair{0xd95eu,0xd98au},std::pair{0xcc27u,0xcc39u},std::pair{0xcc27u,0xcc53u},std::pair{0xaa41u,0xaa4eu},std::pair{0x9cb3u,0x9cc0u},std::pair{0xa4dau,0xa4e7u},std::pair{0xa7b3u,0xa7c0u},std::pair{0xaf72u,0xaf7fu},std::pair{0xadadu,0xadbAu},std::pair{0xa788u,0xa795u},std::pair{0xa322u,0xa32fu},std::pair{0xa293u,0xa2a0u},std::pair{0xcf80u,0xcf8du},std::pair{0xaa82u,0xaa8fu},std::pair{0xab8fu,0xab9cu},std::pair{0xcfc9u,0xcfd6u},std::pair{0xc98eu,0xc9bau},std::pair{0xd340u,0xd34du},std::pair{0xd2d9u,0xd2e6u},std::pair{0xd017u,0xd024u},std::pair{0xd0b9u,0xd0c6u},std::pair{0xd16du,0xd17au},std::pair{0xd13bu,0xd148u},std::pair{0xd72au,0xd737u},std::pair{0xd78fu,0xd7a1u},std::pair{0xd1ddu,0xd1eau},std::pair{0xd1a3u,0xd1b0u},std::pair{0xd5a7u,0xd5b4u},std::pair{0xd6fdu,0xd70au},std::pair{0xd617u,0xd624u},std::pair{0xb6eeu,0xb6fbu}}) {
  put(ram,0x79b,sceneId.first);put(ram,0x7bb,sceneId.second);
  for(unsigned phase:{0u,1u,5u,7u}) {
  put(ram,0x919,phase*256);put(ram,0x923,phase*256);
  reg(regs,Reg::bg2_voffset,0);reg(regs,Reg::bg2_hoffset,411+(phase?0:512));
  for(unsigned ringTile:{3u,1u,2u}) {
    for(unsigned i=0;i<1024;i++)put(vram,0x9000+i*2,ringTile);
    auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
    RowTimeline row(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,100);row.finish();
    for(int x=-43;x<299;x++) {
      auto p=row.pixel(x);auto color=411+x<640?std::array<uint8_t,3>{0,248,0}:std::array<uint8_t,3>{248,0,0};
      if(!p.valid || p.rgb!=color)throw std::runtime_error("room BG2 depends on stale ring or wrong parallax");
    }
  }
  }
  }
  for(auto departure:{std::pair{0xa788u,0xa795u},std::pair{0xcfc9u,0xcfd6u},std::pair{0xd0b9u,0xd0c6u}}) {
  put(ram,0x79b,departure.first);put(ram,0x7bb,departure.second);put(ram,0x919,0);put(ram,0x923,0);
  for(unsigned game:{9u,11u})for(unsigned phase:{0xe17du,0xe29eu,0xe2dbu,0xe2f7u}){
    put(ram,0x998,game);put(ram,0x99c,phase);
    SceneGeometry fade(ram,ram,{},vram,pal,0,false,0,0);
    auto p=fade.background(-43,100,411,0,0,549,0);
    Terrain expected(ram,vram,pal,0,1);auto reference=expected.pixel(411-43,100);
    if(!p || p->color!=reference.color)throw std::runtime_error("Cathedral departure lost full-room BG2");
  }
  put(ram,0x998,11);put(ram,0x99c,0xe310);
  SceneGeometry later(ram,ram,{},vram,pal,0,false,0,0);
  if(later.background(-43,100,411,0,0,549,0))throw std::runtime_error("Cathedral departure extended into header change");
  }
  put(ram,0x998,8);put(ram,0x79b,0xa4da);put(ram,0x7bb,0xa4e8);
  auto owner=std::make_shared<const OwnerImage>(2,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
  RowTimeline other(scene,memoryImage(vram,65536),memoryImage(pal,512),regs,100);other.finish();
  if(other.pixel(-43).rgb!=std::array<uint8_t,3>{248,0,0})throw std::runtime_error("unrelated room state changed");
  std::cout<<"PASS:BG2samples at reported camera, three ring contents, full-room parallax and unrelated-state guard\n";
}
