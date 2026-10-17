#include "raster_timeline.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace SMPWide;
int main(){try {
 std::vector<uint8_t> ram(131072),vram(65536),palette(512);
 auto put=[](auto& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;};
 auto set=[&](unsigned a,unsigned v){put(ram,a,v);};
 auto check=[](bool v,const char* why){if(!v)throw std::runtime_error(why);};
 set(0x79b,0xdd58);set(0x7bb,0xdd6e);set(0x7a5,64);set(0x7a7,16);
 set(0xf78,0xec7f);set(0xf7a,107);set(0x7800,4);set(0x59,0x48);
 set(0x8036,0xc4c9);set(0x943,0x8006);
 // A6:C4A0 writes the first English glyph at BG2 tile (5,8).
 put(vram,(0x4800+8*32+5)*2,1);vram[32]=128;put(palette,2,31);
 auto sample=[&](int x){SceneGeometry g(ram,ram,{},vram,palette,0,false,0,0);return g.background(x,64,0,0,0,0,0);};
 for(unsigned game:{8u,9u,11u})for(unsigned phase:{0xe17du,0xe29eu,0xe2dbu,0xe2f7u}) {
  set(0x998,game);set(0x99c,phase);
  auto native=sample(40),repeat=sample(296);
  check(native && native->opaque && native->color==31,"native caption glyph changed");
  check(repeat && repeat->valid && !repeat->opaque,"caption repeats during departure");
 }
 set(0x998,11);set(0x99c,0xe310);check(!sample(296),"caption clipping survived outgoing phase");
 set(0x998,9);
 for(unsigned address:{0x79bu,0x7bbu,0xf78u,0x8036u,0x943u}) {
  unsigned old=word(ram,address);set(address,0);check(!sample(296),"caption guard leaked");set(address,old);
 }
 auto kind=[&](SceneKind k=SceneKind::Staging){auto owner=std::make_shared<const OwnerImage>(1,ram);return coordinateKind({owner,owner,{},k});};
 for(unsigned game:{9u,11u})for(unsigned phase:{0xe17du,0xe29eu,0xe2dbu,0xe2f7u}) {
  set(0x998,game);set(0x99c,phase);
  for(int shake=-3;shake<=3;shake++)check(sceneCoordinate(unsigned(shake)&65535,0,kind())==shake,"departure quake wrapped outside room");
 }
 set(0x998,11);set(0x99c,0xe310);check(sceneCoordinate(65535,0,kind())==65535,"destination header wrapping changed");
 set(0x99c,0xe2db);set(0x79b,0xde4d);check(kind()==SceneKind::Staging,"unrelated room coordinate changed");
 set(0x79b,0xdd58);set(0x911,256);check(kind()==SceneKind::Staging,"scrolling camera coordinate changed");set(0x911,0);
 for(auto k:{SceneKind::Right,SceneKind::Left,SceneKind::Down,SceneKind::Up,SceneKind::ReadyBeforeScroll})check(kind(k)==k,"joined door wrapping changed");
 std::cout<<"PASS native caption glyph, non-repeating departure fade and ownership guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
