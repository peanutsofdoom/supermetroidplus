#include "cinematic_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace SMPWide;
int main(){try{
 std::vector<uint8_t> v(65536),p(512),o(544),r(205),out(342*224*3,123),ram(131072),table(256);
 auto put=[&](Reg k,unsigned n){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=n>>(8*i);};
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 put(Reg::bgMode,7);put(Reg::m7a,256);put(Reg::m7d,256);put(Reg::aboveEnable,1);put(Reg::priority0,1);put(Reg::displayBrightness,15);
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)v[2*(y*8+x)+1]=x+1;
 for(unsigned x=1;x<=8;x++)p[2*x]=x;
 put(Reg::mosaicEnable,1);put(Reg::mosaicSize,1);put(Reg::mosaicCounter,1);
 check(cinematicRow(v,p,o,r,100,0xc9f9,out.data(),2),"inert Mode7 mosaic rejected");
 for(unsigned x=43;x<299;x++)for(unsigned c=0;c<3;c++)check(out[(99*342+x)*3+c]==123,"native center overwritten");
 for(int x:{-43,-1,256,298}){unsigned expected=((((unsigned(x)&7)+1)*15*16+15)/30)<<16;check(CinematicRow(v,p,o,r,100,0xc9f9,2).pixel(x)==expected,"affine margin sample wrong");}
 put(Reg::mosaicSize,2);check(!cinematicRow(v,p,o,r,100,0,out.data(),2),"unsupported Mode7 mosaic accepted");put(Reg::mosaicSize,1);
 put(Reg::window_col_oneEnable,1);check(!cinematicRow(v,p,o,r,100,0,out.data(),2),"foreign first window accepted");put(Reg::window_col_oneEnable,0);
 put(Reg::window_col_twoEnable,1);check(!cinematicRow(v,p,o,r,100,0,out.data(),2),"unwitnessed second window accepted");put(Reg::window_col_twoEnable,0);
 put(Reg::screen_bg1_colorEnable,1);put(Reg::screen_colorBlue,31);put(Reg::window_col_belowMask,1);
 check((CinematicRow(v,p,o,r,100,0,2).pixel(-1)&255)==0,"disabled color-window mask ignored");
 EyeState eye;eye.configure(table);auto wordPut=[&](unsigned a,unsigned n){ram[a]=n;ram[a+1]=n>>8;};
 for(unsigned game:{34u,37u})for(unsigned function:{0xc610u,0xc627u,0xc699u}){wordPut(0x998,game);wordPut(0x1f51,function);wordPut(0x592,0);eye.reset();eye.owner(ram);check(eye.ceresPowerBombTail(),"native explosion tail rejected");wordPut(0x592,0x8000);eye.reset();eye.owner(ram);check(!eye.ceresPowerBombTail(),"active power bomb treated as tail");}
 wordPut(0x592,0);wordPut(0x998,8);eye.reset();eye.owner(ram);check(!eye.ceresPowerBombTail(),"gameplay treated as explosion tail");wordPut(0x998,34);wordPut(0x1f51,0xc345);eye.reset();eye.owner(ram);check(!eye.ceresPowerBombTail(),"foreign cinematic phase treated as tail");
 std::cout<<"PASS cinematic affine margins, native-center exclusion, inert mosaic, color/window and explosion-tail guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
