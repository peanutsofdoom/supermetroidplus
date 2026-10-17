#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
static unsigned romAt(unsigned bank,unsigned p){return (bank&127)*32768+(p&32767);}
int main(){try{
 std::vector<uint8_t> ram(131072),rom(3145728),vram(65536),pal(512),regs(205);
 put(ram,0x998,8);put(ram,0x79b,0xdd58);put(ram,0x7bb,0xdd6e);
 put(ram,0xf78,0xec7f);put(ram,0xfb8,0xec3f);ram[0xfa6]=ram[0xfe6]=0xa9;
 put(ram,0xf86,0x100);put(ram,0xfc6,0x100);put(ram,0x178c,0x87dd);ram[0x178e]=0xa9;
 put(ram,0x8002,0x9000);put(rom,romAt(0xa9,0x9000),1);put(rom,romAt(0xa9,0x9002),0x9100);
 unsigned map=romAt(0xa9,0x9100);put(rom,map,1);put(rom,map+2,0x8000);put(rom,map+5,0x200);
 put(ram,0xfba,100);put(ram,0xfbe,60);put(ram,0x911,132);put(ram,0x781a,0x200);
 for(unsigned y=0;y<8;y++)vram[y*2]=255;
 put(pal,(128+16+1)*2,31);put(pal,(128+48+1)*2,992);
 auto head=[&](int x,unsigned line){return EnemySampler(ram,rom,vram,pal,regs).pixel(x,line);};
 check(head(-32,61) && head(-32,61)->color==31,"custom head missing in margin");
 check(!head(0,61),"custom head changed native center");
 put(rom,map,2);put(rom,map+7,16);rom[map+9]=0;put(rom,map+10,0x400);
 ram[0x370]=224;ram[0x371]=60;put(ram,0x372,0x200);ram[0x570]=3;
 check(head(-16,61) && head(-16,61)->color==992,"matching head witness flattened map palettes");put(rom,map,1);
 // A native OAM witness anchors draw-time shake and palette.
 ram[0x370]=225;ram[0x371]=61;put(ram,0x372,0x600);ram[0x570]=3;
 check(head(-31,62) && head(-31,62)->color==992,"head witness lost shake or palette");
 check(!head(-32,61),"head witness left old origin behind");
 put(ram,0x178c,0x8786);check(!head(-31,62),"inactive custom hook drew head");
 put(ram,0x178c,0x87dd);put(ram,0x7bb,0xdd6f);check(!head(-31,62),"unrelated state drew head");put(ram,0x7bb,0xdd6e);
 put(rom,romAt(0xa9,0x9000),0x8000);check(!head(-31,62),"animation instruction treated as map");
 // One non-repeating body map; retain real edge pixels, reject wrapped copies.
 put(ram,0x7a5,64);put(ram,0x7a7,16);put(ram,0x911,0);put(ram,0xf7a,60);put(ram,0x7800,2);ram[0x59]=0x48;
 for(unsigned i=0;i<1024;i++)put(vram,0x9000+i*2,0);
 put(pal,2,31);
 SceneGeometry body(ram,ram,{},vram,pal,0,false,0,0);
 auto edge=body.background(256,32,230,0,0,0,0),repeat=body.background(284,32,230,0,0,0,0);
 check(edge && edge->valid && edge->opaque,"real body-map edge rejected");
 check(repeat && repeat->valid && !repeat->opaque,"body repeated beyond map extent");
 put(ram,0x7800,1);put(ram,0x178c,0x87c9);SceneGeometry rise(ram,ram,{},vram,pal,0,false,0,0);check(!rise.background(284,32,230,0,0,0,0)->opaque,"body repeated during its introduction");
 put(ram,0x7800,0);SceneGeometry initial(ram,ram,{},vram,pal,0,false,0,0);check(!initial.background(284,32,230,0,0,0,0),"body clamp applied during first phase");
 // Climb filler only. Native center, authored details, and other rooms remain.
 ram.assign(131072,0);put(ram,0x998,8);put(ram,0x79b,0x96ba);put(ram,0x7bb,0x96eb);put(ram,0x7a5,48);put(ram,0x7a7,144);put(ram,0x911,256);
 for(unsigned q=0;q<4;q++){put(ram,0xa000+0x2e1*8+q*2,0);put(ram,0xa000+0x3ff*8+q*2,1);}
 for(unsigned y=0;y<144;y++)for(unsigned x=0;x<48;x++)put(ram,0x10002+(y*48+x)*2,0x83ff);
 SceneGeometry wall(ram,ram,{},vram,pal,0,false,0,0);
 check(wall.pixel(213,1600).opaque && wall.pixel(554,1600).opaque,"shaft filler remains blank");
 check(!wall.pixel(256,1600).opaque && !wall.pixel(511,1600).opaque,"shaft fix entered native center");
 check(!wall.pixel(212,1600).opaque && !wall.pixel(555,1600).opaque,"shaft fix exceeded wide margins");
 put(ram,0x998,9);SceneGeometry fadeWall(ram,ram,{},vram,pal,0,false,0,0);check(fadeWall.pixel(554,1600).opaque,"wall vanished during departure");
 put(ram,0x998,11);put(ram,0x99c,0xe310);SceneGeometry loadedWall(ram,ram,{},vram,pal,0,false,0,0);check(!loadedWall.pixel(554,1600).opaque,"wall survived header change");put(ram,0x998,8);
 put(ram,0x10002+(100*48+13)*2,0x83fe);SceneGeometry detail(ram,ram,{},vram,pal,0,false,0,0);check(detail.pixel(213,1600).color==Terrain(ram,vram,pal,0).pixel(213,1600).color,"authored detail replaced");
 put(ram,0x79b,0x96bb);SceneGeometry other(ram,ram,{},vram,pal,0,false,0,0);check(!other.pixel(554,1600).opaque,"unrelated room filled");
 // Same live Kihunter descriptor through the pre-header departure fade.
 ram.assign(131072,0);put(ram,0x79b,0x948c);put(ram,0x7bb,0x9499);put(ram,0xf78,0xeabf);put(ram,0xf7a,65526);put(ram,0xf7e,60);put(ram,0xf82,8);put(ram,0xf8e,0x9000);ram[0xfa6]=0xa8;
 map=romAt(0xa8,0x9000);put(rom,map,1);put(rom,map+2,0x8000);put(rom,map+5,0x200);
 auto wing=[&](){return EnemySampler(ram,rom,vram,pal,regs).pixel(-10,61);};
 for(unsigned game:{8u,9u,0x1bu}){put(ram,0x998,game);check(bool(wing()),"enemy disappeared in gameplay/refill/departure");}
 put(ram,0x998,11);for(unsigned phase:{0xe17du,0xe29eu,0xe2dbu,0xe2f7u}){put(ram,0x99c,phase);check(bool(wing()),"enemy disappeared during qualified fade");}
 put(ram,0x99c,0xe310);check(!wing(),"enemy survived destination header change");
 put(ram,0x998,9);put(ram,0x79b,0x948d);check(!wing(),"enemy departure scope widened");
 std::cout<<"PASS custom brain maps/witnesses, nonrepeating body, shaft filler guards, reserve and enemy-fade scope\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
