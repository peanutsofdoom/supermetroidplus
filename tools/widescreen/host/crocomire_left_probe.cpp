#include "experimental_activation.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
int main(){std::vector<unsigned> r(131072);auto word=[&](unsigned a){return r[a];};r[0x998]=8;r[0x79b]=0xa98d;r[0xf78]=0xddbf;r[0x7a5]=128;r[0xfa8]=0x40;
unsigned cases=0;
for(bool spike:{true,false}){for(unsigned row=3;row<=10;row++){unsigned b=0x10002+row*256;r[b+68]=spike?0xa49c:0x109;r[b+64]=spike?0x8140:0x8109;r[b+60]=spike?0x80:0x8108;r[b+66]=spike?0x8102:0x108;}
for(int x=400;x<600;x++)for(unsigned row=0;row<16;row++){unsigned block=row*128+x/16;auto w=smpCrocomireLeftWall(word,block);unsigned old=(row>=3&&row<=10&&x>=(spike?512:480)&&x<(spike?560:528))?0x8000:0;unsigned got=smpCrocomireLeftType(w,block,old,x,x+1)&0xf000;bool solid=row>=3&&row<=10&&x>=(spike?469:437)&&x<(spike?517:485);if(bool(got)!=solid || (solid&&spike&&x>=501&&got!=0xa000))throw std::runtime_error("Left wall extent/type");cases++;}}
r[0x79b]=0xa923;if(smpCrocomireLeftWall(word,5*128+30))throw std::runtime_error("Other room");std::cout<<"PASS "<<cases<<" shifted wall/spike/invisible boundary cases\n";}
