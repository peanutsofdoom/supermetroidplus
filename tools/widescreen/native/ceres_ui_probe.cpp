#include "frame_pipeline.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try {
 auto check=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),r(205);
 put(ram,0x998,8);put(ram,0x79b,0xe0b5);put(ram,0x7bb,0xe0c7);put(ram,0x79f,6);
 put(ram,0x7a5,32);put(ram,0x7a7,16);put(ram,0x7a9,2);put(ram,0xf78,0xe13f);put(ram,0x10002+(6*32+20)*2,0x8000);
 for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
 reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::tiledataAddress,0x6000);reg(r,Reg::screenAddress,0x5000);reg(r,Reg::screenSize,1);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority0,2);reg(r,Reg::priority1,5);
 reg(r,Reg::obj_tiledataAddress,0x6000);reg(r,Reg::obj_aboveEnable,1);reg(r,Reg::obj_priority3,8);
 put(p,0,31);put(p,97*2,31<<5);put(p,(128+5*16+1)*2,31<<10);
 for(unsigned y=0;y<8;y++){v[0xc000+0x186*32+y*2]=255;v[0xc000+0x1e0*32+y*2]=255;}
 put(v,(0x5000+6*32+11)*2,0x3986);
 auto sample=[&](int x,unsigned line,bool fill,int shift){
  auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
  RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,line,{}, {},{},memoryImage(rom,3145728));row.finish();return row.pixel(x,fill,shift);
 };
 auto green=sample(88,49,false,0);check(green.valid && green.rgb[1]>0 && !green.rgb[0],"native caption reference lost");
 auto red=sample(88,49,true,43);check(red.valid && red.rgb[0]>0 && !red.rgb[1],"vacated caption did not restore room");
 check(sample(131,49,true,43).rgb==green.rgb,"caption did not move exactly43pixels");
 put(v,(0x5000+6*32+11)*2,0);put(v,(0x5000+8*32+26)*2,0x3586);put(p,81*2,31<<5);
 check(sample(251,65,true,43).rgb[1]>0,"last SEQUENCE glyph clipped at old edge");
 put(ram,0x943,0x8006);ram[0x949]=220;ram[0x94b]=108;ram[0x370]=188;ram[0x371]=100;put(ram,0x372,0x3be0);
 auto blue=sample(188,101,false,0);check(blue.rgb[2]>0 && !blue.rgb[0],"native timer reference lost");
 check(sample(231,101,true,43).rgb==blue.rgb,"timer did not follow centered HUD");
 check(sample(188,101,true,43).rgb==red.rgb,"old timer left a duplicate");
 ram[0x949]=20;check(sample(188,101,true,43).rgb==blue.rgb,"unrelated sprite moved without timer-position witness");
 std::cout<<"PASS Ceres caption source/destination, last letter, native timer priority, no duplicate and position guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
