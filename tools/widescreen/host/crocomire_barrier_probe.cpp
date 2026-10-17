#include "experimental_activation.hpp"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
int main(){
 unsigned cases=0;
 for(unsigned row=0;row<16;row++)for(int x=0;x<1300;x++){
  unsigned block=row*128+unsigned(x/16),tile=(row>=3&&row<=10&&x>=768&&x<816)?0x8123:0x0123;
  auto got=smpCrocomireBarrierType(block,tile,x,x+1);
  bool expected=row>=3&&row<=10&&x>=811&&x<859;
  if(bool(got&0x8000)!=expected || (got&0xfff)!=0x123)throw std::runtime_error("Point barrier translation");cases++;
 }
 for(int center=735;center<890;center++)for(int radius:{1,5,7,16})for(unsigned col=45;col<57;col++){
  unsigned block=5*128+col,tile=col>=48&&col<=50?0x8107:0x0107;bool solid=false;
  for(int x=center-radius;x<center+radius;x++)if(x>=int(col*16)&&x<int(col*16+16)&&x>=811&&x<859)solid=true;
  if(bool(smpCrocomireBarrierType(block,tile,center-radius,center+radius)&0x8000)!=solid)throw std::runtime_error("Vertical span barrier");cases++;
 }
 std::vector<uint8_t> ram(131072);auto put=[&](unsigned a,unsigned v){ram[a]=v;ram[a+1]=v>>8;};auto word=[&](unsigned a){return ram[a]|unsigned(ram[a+1])<<8;};
 for(auto p:{std::pair{0x998,8},std::pair{0x79b,0xa98d},std::pair{0xf78,0xddbf},std::pair{0x7a5,128},std::pair{0xfa8,0x40},std::pair{0x10362,0x8080},std::pair{0x10462,0x8107}})put(p.first,p.second);
 if(!smpCrocomireBarrierActive(word))throw std::runtime_error("Active wall missing");
 for(auto p:{std::pair{0x998,9},std::pair{0x79b,0xa98e},std::pair{0xf78,0},std::pair{0x7a5,64},std::pair{0xfa8,0x4a},std::pair{0x10362,0x80}}){unsigned old=word(p.first);put(p.first,p.second);if(smpCrocomireBarrierActive(word))throw std::runtime_error("Wall scope leaked");put(p.first,old);}
 std::cout<<"PASS "<<cases<<" point/span cases; original position cleared, exact43px translation and lifetime guards\n";
}
