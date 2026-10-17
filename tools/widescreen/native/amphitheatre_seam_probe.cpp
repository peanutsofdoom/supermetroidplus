#include "raster_timeline.hpp"
#include <cassert>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){
 std::vector<uint8_t> ram(131072),v(65536),p(512),r(205),rom(3145728);
 auto reg=[&](Reg k,unsigned value){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=value>>(i*8);};
 for(auto q:{std::pair{0x79b,0xb4e5},std::pair{0x7bb,0xb4f2},std::pair{0x998,8},std::pair{0x7a5,64},std::pair{0x7a7,80},std::pair{0x911,768},std::pair{0x183e,0x15},std::pair{0x1840,30}})put(ram,q.first,q.second);
 for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
 for(unsigned i=0;i<1024;i++)put(v,0x9000+i*2,1);
 for(unsigned y=0;y<8;y++)v[32+y*2]=255;
 put(p,2,31);reg(Reg::bgMode,1);reg(Reg::displayBrightness,15);reg(Reg::aboveEnable,1);reg(Reg::priority0,4);reg(Reg::screenAddress,0x4800);reg(Reg::hoffset,770);
 auto row=[&](){auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection s{owner,owner,{},SceneKind::Ordinary};RowTimeline q(s,memoryImage(v,65536),memoryImage(p,512),r,100,{}, {},{},memoryImage(rom,3145728));q.finish();return q;};
 auto a=row();assert(a.hasCenterFill());assert(!a.centerCandidate(253));assert(a.centerCandidate(254)&&a.centerCandidate(255));
 for(int x:{254,255}){assert((a.pixel(x).rgb==std::array<uint8_t,3>{248,0,0}));assert(a.pixel(x,true).valid);assert((a.pixel(x,true).rgb==std::array<uint8_t,3>{0,0,0}));}
 ram[0x370]=254;ram[0x371]=99;put(ram,0x372,1);assert(!row().centerCandidate(254));ram[0x371]=240;
 for(unsigned wrong:{0u,0x14u,0x16u}){put(ram,0x183e,wrong);assert(!row().hasCenterFill());}put(ram,0x183e,0x15);
 put(ram,0x1840,0);assert(!row().hasCenterFill());put(ram,0x1840,30);
 put(ram,0x7bb,0xb4f3);assert(!row().hasCenterFill());put(ram,0x7bb,0xb4f2);
 put(ram,0x911,767);assert(!row().hasCenterFill());put(ram,0x911,768);
 for(unsigned scroll:{766u,768u,769u,771u}){reg(Reg::hoffset,scroll);assert(!row().hasCenterFill());}
 std::cout<<"PASS quake right columns, black room boundary, native-object exclusion and exact scene/phase guards\n";
}
