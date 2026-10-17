#include "crocomire_scare.hpp"
#include <array>
#include <vector>
#include <iostream>
#include <stdexcept>
int main(){std::array<unsigned,131072> r{};std::vector<unsigned> rom(3145728);unsigned writes=0;
auto read=[&](unsigned a){return r[a];};auto write=[&](unsigned a,unsigned v){r[a]=v;writes++;};auto rr=[&](unsigned a){return rom[a];};
auto reset=[&](){r.fill(0);r[0x998]=8;r[0x79b]=0xa98d;r[0xf78]=0xddbf;r[0xfa8]=0x44;r[0xaf6]=500;r[0xafa]=100;r[0xafe]=5;r[0xb00]=16;r[0x1997]=0x90c1;r[0x1b6b]=0x8000;r[0x1a4b]=543;r[0x1a93]=100;r[0x9c0]=1;r[0x9d6]=300;rom[0x68000]=1;rom[0x68003]=0x80;writes=0;};
for(unsigned hp:{1u,2u,99u,999u,1499u}){reset();r[0x9c2]=hp;smpCrocoScareTick(read,rr,write);if(r[0x9c2]!=1 || !smpCrocoScareProtected(read) || r[0x9d6]!=300)throw std::runtime_error("Initial nonlethal hit");
for(unsigned damage:{0u,1u,300u,65535u}){r[0x9c2]=damage==0?1:0;smpCrocoScareFloor(read,write);if(r[0x9c2]!=1||r[0x9d6]!=300)throw std::runtime_error("Lethal follow-up or reserve drained");}
r[0x9c2]=50;for(int i=0;i<600;i++)smpCrocoScareTick(read,rr,write);if(r[0x9c2]!=50)throw std::runtime_error("Repeated hit removed healing");auto saved=r;r.fill(0);r=saved;r[0x9c2]=0;smpCrocoScareFloor(read,write);if(r[0x9c2]!=1)throw std::runtime_error("Restore lost protection");r[0xfa8]=0x52;r[0xa78]=1;auto state=r[SmpCrocoScareState];for(int i=0;i<300;i++)smpCrocoScareTick(read,rr,write);if(r[SmpCrocoScareState]!=state)throw std::runtime_error("Frozen countdown");r[0xa78]=0;for(int i=0;i<120;i++)smpCrocoScareTick(read,rr,write);if(smpCrocoScareProtected(read))throw std::runtime_error("Protection did not expire");r[0x9c2]=0;smpCrocoScareFloor(read,write);if(r[0x9c2])throw std::runtime_error("Ordinary later death prevented");}
for(auto bad:{std::pair{0x79b,0xa98e},std::pair{0xfa8,0x42},std::pair{0x1997,0xa17b},std::pair{0x998,19},std::pair{0xa78,1},std::pair{0xaf6,543},std::pair{0x9c2,0}}){reset();r[0x9c2]=999;r[bad.first]=bad.second;smpCrocoScareTick(read,rr,write);if(writes)throw std::runtime_error("Scope/non-contact changed");}
std::cout<<"PASS1HP/repeated-hit/lethal follow-up/reserve/healing/restore/freeze/expiry and scope guards\n";}
