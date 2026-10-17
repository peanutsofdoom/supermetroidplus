#include "playthrough_eligibility.hpp"
#include "scene_eligibility.hpp"
#include <stdexcept>
#include <iostream>
void require(bool v){if(!v)throw std::runtime_error("policy mismatch");}
int main(){
  SMPPlaythroughEligibility p;
  require(!p.allow(11,0xe38e,0x92fd,0x932e));
  for(unsigned room=0x8000;room<=0xffff;room++) {
    p.reset();require(!p.allow(6,0,room,0xffff));
    require(p.allow(7,0,room,0xffff));
    require(p.allow(8,0,room,0xffff));
    p.reset();require(p.allow(0x0c,0,room,0xffff));
    require(!p.allow(0x0d,0,room,0xffff));require(!p.allow(0x0e,0,room,0xffff));
    p.reset();require(p.allow(0x12,0,room,0xffff));
    p.reset();require(p.allow(0x29,0,room,0xffff));require(p.allow(0x2a,0,room,0xffff));
    require(!p.allow(0x28,0,room,0xffff));require(!p.allow(0x2b,0,room,0xffff));require(!p.allow(0x2c,0,room,0xffff));
    for(unsigned death=0x13;death<=0x19;death++)require(p.allow(death,0,room,0xffff));
    require(!p.allow(0x1a,0,room,0xffff));
    p.reset();require(p.allow(0x1b,0,room,0xffff));require(!p.allow(0x1c,0,room,0xffff));
    p.reset();require(p.allow(0x26,0,room,0xffff));require(!p.allow(0x27,0,room,0xffff));
    require(!p.allow(0x10,0,room,0xffff));require(!p.allow(0x11,0,room,0xffff));
    require(p.allow(9,0,room,0xffff));
    require(p.allow(11,0xe38e,room,0xffff));
    require(!p.allow(11,0xffff,room,0xffff));
    require(!p.allow(15,0,room,0xffff));
  }
  p.reset();require(!p.allow(9,0,0x91f8,0x922d));
  require(!p.allow(8,0,0,0));
  SMPGuiEligibility strict;
  require(!strict.allow(8,0,0x92fd,0x932e));
  require(p.allow(8,0,0x92fd,0x932e));
  std::cout<<"PASS: all room headers enabled; menus, unknown phases and missing history rejected; strict policy preserved\n";
}
