#include "ceres_arena.hpp"
#include "cinematic_eligibility.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
int main(){try {
 std::vector<unsigned char> ram(131072);
 auto read=[&](unsigned a){return unsigned(ram[a])|(unsigned(ram[a+1])<<8);};
 auto byte=[&](unsigned a,unsigned v){ram[a]=v;};
 auto put=[&](unsigned a,unsigned v){byte(a,v);byte(a+1,v>>8);};
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 put(0x79b,0xe0b5);put(0x7bb,0xe0c7);put(0x79f,6);put(0x7a5,16);put(0x7a7,16);put(0x7a9,1);put(0xf78,0xe13f);put(0x8006,224);
 for(unsigned n=0;n<256;n++){put(0x10002+n*2,(n%16==15?0x8000:0)|(n&1023));put(0x19602+n*2,0x3000+n);byte(0x16402+n,n^93);}
 put(0x1c37,0xbeef);put(0x1c87,2*(6*16+15));put(0x1c39,0xbeee);put(0x1c89,2*(8*16+3));put(0x1c3b,0xbeed);put(0x1c8b,600);byte(0xcd21,1);
 auto original=ram;
 for(unsigned a:{0x79bu,0x7bbu,0x79fu,0x7a5u,0x7a7u,0x7a9u,0xf78u}){ram=original;put(a,read(a)+1);auto before=ram;check(!smpExpandCeresArena(read,put,byte),"foreign room/state/geometry/boss accepted");check(ram==before,"guard mutated memory");}
 ram=original;check(smpExpandCeresArena(read,put,byte),"expansion refused");auto actual=ram;ram=original;
 // Independent expected memory image: retain left fifteen columns, add five
 // floor/air columns, relocate the wall, and seal unused storage columns.
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<32;x++){
  unsigned s=y*16+(x<=14?x:x<=19?14:15),d=y*32+x;
  auto old=[&](unsigned a){return unsigned(original[a])|(unsigned(original[a+1])<<8);};
  put(0x10002+d*2,x<=20?old(0x10002+s*2):0x83ff);
  put(0x19602+d*2,x<=20?old(0x19602+s*2):0xff);byte(0x16402+d,x<=20?original[0x16402+s]:0);
 }
 put(0x1c87,2*(6*32+15));put(0x1c89,2*(8*32+3));put(0x10000,1024);put(0x7a5,32);put(0x7a9,2);put(0x8006,304);byte(0xcd21,0);
 check(ram==actual,"unexpected write or incorrect geometry/BTS/PLM delta");check(smpCeresArena(read),"expanded arena detection failed");check(!smpExpandCeresArena(read,put,byte),"expansion repeated");check(ram==actual,"second pass mutated memory");
 // The post-fight room has no Ridley slot; its geometry must still expand.
 ram=original;put(0x7bb,0xe0e1);put(0xf78,0);auto post=ram;
 check(smpExpandCeresArena(read,put,byte),"post-fight return stayed narrow");
 check(smpCeresArena(read),"post-fight wide room not recognized");
 check(read(0x8006)==224,"post-fight expansion changed an absent actor's memory");
 auto once=ram;check(!smpExpandCeresArena(read,put,byte) && ram==once,"post-fight expansion repeated");
 ram=actual;
 unsigned a=0xfb8;put(a,0xe23f);put(a+60,6);put(a+2,248);auto before=ram;smpMoveCeresWallActor(read,put);check(read(a+2)==328,"getaway wall not moved");before[a+2]=72;before[a+3]=1;check(ram==before,"wall actor changed unrelated memory");smpMoveCeresWallActor(read,put);check(ram==before,"wall moved twice");
 for(unsigned field:{0u,60u}){ram=before;put(a+2,248);put(a+field,0);auto foreign=ram;smpMoveCeresWallActor(read,put);check(ram==foreign,"foreign actor moved");}
 check(smpCinematicScene(30,0xbfda,0,0)==2,"intro flythrough excluded");check(!smpCinematicScene(30,0xb800,0,7),"story text included");check(smpCinematicScene(34,0,0,1)==2,"explosion excluded");check(smpCinematicScene(37,0,0,1)==2,"failed escape excluded");check(smpCinematicScene(39,0,0,1)==1,"ending excluded");
 for(unsigned room:{0xdf45u,0xdf8du,0xdfd7u,0xe021u,0xe06bu,0xe0b5u}){check(smpCinematicScene(8,0,room,7)==3,"station Mode7 excluded");check(!smpCinematicScene(8,0,room,1),"ordinary room misclassified");}
 check(smpCinematicScene(11,0,0xdf45,1)==3,"midframe elevator switch excluded");check(!smpCinematicScene(8,0,0x91f8,7),"foreign Mode7 room accepted");check(!smpCinematicScene(12,0,0xe0b5,7),"pause accepted");
 std::cout<<"PASS Ceres full-memory expansion delta, geometry/BTS/PLMs, camera/Ridley limit, idempotence, wall actor and cinematic eligibility guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
