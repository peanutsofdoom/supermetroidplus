#include "mother_pillar_reveal.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),r(205),rgb(342*224*3,13);
 put(ram,0x998,8);put(ram,0x79b,0xdd58);put(ram,0x7bb,0xdd6e);put(ram,0x7a5,64);put(ram,0x7a7,16);put(ram,0xf78,0xec7f);put(ram,0xfb8,0xec3f);put(ram,0x7800,1);put(ram,0xfa8,0x88d3);put(ram,0x10002+(6*64+15)*2,0x830f);put(ram,0x10002+(6*64+20)*2,0xff);
 MotherPillarReveal fx;auto before=ram;fx.observe(ram);fx.retain(rgb);check(ram==before && !fx.active(),"ordinary scene changed or triggered");
 put(ram,0xfa8,0x8c87);fx.observe(ram);check(fx.active() && fx.frame()==0,"native preparation did not start reveal");
 for(unsigned i=0;i<6;i++){put(rom,0x36208+i*4,5);put(rom,0x36208+i*4+2,0xb406);}
 put(rom,0x6b406,1);put(rom,0x6b408,0);rom[0x6b40a]=0;put(rom,0x6b40b,0x3a00);
 reg(r,Reg::displayBrightness,15);put(p,209*2,31<<5);for(unsigned y=0;y<8;y++)v[y*2]=255;
 auto untouched=rgb;fx.row(rgb,48,rom,v,p,r,43);check(rgb[(48*342+288)*3+1]>200,"native explosion map not decoded");check(std::equal(rgb.begin(),rgb.begin()+31*342*3,untouched.begin()),"explosion altered HUD");
 fx.shake(rgb);check(std::equal(rgb.begin(),rgb.begin()+31*342*3,untouched.begin()),"shake altered HUD");
 put(ram,0xfa8,0x8d79);put(ram,0x10002+(6*64+15)*2,0);put(ram,0x10002+(6*64+20)*2,0x830f);
 for(unsigned i=1;i<48;i++){fx.observe(ram);check(fx.active() && fx.frame()==int(i),"effect duration skipped");}
 fx.observe(ram);check(!fx.active(),"effect failed to expire");auto end=rgb;fx.row(rgb,48,rom,v,p,r,0);fx.shake(rgb);check(rgb==end,"expired effect still rendered");
 fx.reset();fx.observe(ram);check(!fx.active(),"expanded checkpoint replayed blast");
 put(ram,0x10002+(6*64+15)*2,0x830f);put(ram,0x10002+(6*64+20)*2,0xff);put(ram,0xfa8,0x8c87);fx.reset();fx.observe(ram);fx.observe(ram);check(!fx.active(),"mid-reveal checkpoint replayed blast");
 put(ram,0x79b,0xe0b5);fx.reset();fx.observe(ram);fx.observe(ram);check(!fx.active(),"foreign room triggered blast");
 std::cout<<"PASS pillar trigger, native sprite decoding,48-frame lifetime, stable HUD, scene and checkpoint guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
