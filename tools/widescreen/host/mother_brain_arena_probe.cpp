#include "mother_brain_arena.hpp"
#include <vector>
#include <stdexcept>
#include <iostream>
int main(){try {
 std::vector<unsigned char> ram(131072);
 auto read=[&](unsigned a){return unsigned(ram[a])|(unsigned(ram[a+1])<<8);};
 auto byte=[&](unsigned a,unsigned v){ram[a]=v;};
 auto put=[&](unsigned a,unsigned v){byte(a,v);byte(a+1,v>>8);};
 auto check=[](bool v,const char* s){if(!v)throw std::runtime_error(s);};
 put(0x79b,0xdd58);put(0x7bb,0xdd6e);put(0x7a5,64);put(0x7a7,16);put(0xf78,0xec7f);put(0xfb8,0xec3f);
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<64;x++){put(0x10002+(y*64+x)*2,(x==15?0x8000:0)|((y*37+x)&1023));byte(0x16402+y*64+x,y*13+x);}
 put(0x10002+(6*64+15)*2,0x830f);put(0x10002+(6*64+20)*2,0xff);
 put(0x1c37,0xb673);put(0x1c87,(7*64+15)*2);put(0x1c39,0xb673);put(0x1c89,(7*64+14)*2);put(0x1c3b,0xb674);put(0x1c8b,(8*64+15)*2);
 auto original=ram;
 for(unsigned addr:{0x79bu,0x7bbu,0x7a5u,0x7a7u,0xf78u,0xfb8u}){ram=original;put(addr,read(addr)+1);auto before=ram;check(!smpExpandMotherBrainArena(read,put,byte),"wrong room/geometry/boss accepted");check(ram==before,"guard changed memory");}
 ram=original;check(smpExpandMotherBrainArena(read,put,byte),"expansion refused");check(smpMotherBrainArena(read),"expanded detection failed");auto expected=original;
 for(unsigned y=0;y<16;y++)for(unsigned x=11;x<=20;x++){unsigned dst=y*64+x,src=dst-5;expected[0x10002+dst*2]=original[0x10002+src*2];expected[0x10003+dst*2]=original[0x10003+src*2];expected[0x16402+dst]=original[0x16402+src];}
 expected[0x1c87]=unsigned((7*64+20)*2)&255;expected[0x1c88]=((7*64+20)*2)>>8;
 check(ram==expected,"unexpected memory write or geometry/BTS/PLM mismatch");
 check(!smpExpandMotherBrainArena(read,put,byte),"expansion repeated");check(ram==expected,"second expansion changed memory");
 for(unsigned addr:{0x10002u+(6*64+15)*2,0x10002u+(6*64+20)*2}){ram=original;put(addr,read(addr)^1);auto before=ram;check(!smpExpandMotherBrainArena(read,put,byte),"unauthored geometry accepted");check(ram==before,"geometry refusal changed memory");}
 std::cout<<"PASS arena geometry/BTS relocation, PLM scope, idempotence, room/boss and authored-cell guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
