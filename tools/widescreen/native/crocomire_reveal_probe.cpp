// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_geometry.hpp"
#include <vector>
#include <iostream>
using namespace SMPWide;
int main() {
  std::vector<uint8_t> r(131072),v(65536),p(512);
  auto put=[&](unsigned a,unsigned n){r[a]=n;r[a+1]=n>>8;};
  put(0x79b,0xa98d);put(0xf78,0xddbf);put(0x998,8);
  put(0x7a5,128);put(0x7a7,16);put(0xfa8,0x40);
  for(unsigned y=3;y<11;y++){put(0x10002+(y*128+32)*2,0x8140);put(0x10002+(y*128+34)*2,0xa49c);}
  auto covered=[&](int x,int y,int screen){SceneGeometry g(r,r,{},v,p,0,false,0,0);return bool(g.crocomireWall(x,y));};
  unsigned count=0;
  for(int x=468;x<=560;x++)for(int y=47;y<=176;y++) {
    if(covered(x,y,x-512)!=(x>=469 && x<560 && y>=48 && y<176))throw std::runtime_error("Cover bounds");
    count++;
  }
  if(!covered(530,100,18))throw std::runtime_error("Vacated wall not cleared");
  for(unsigned state:{8u,12u,18u}){put(0x998,state);if(!covered(480,100,-32))throw std::runtime_error("Pause fade");}
  put(0x998,15);if(covered(480,100,-32))throw std::runtime_error("Map covered");put(0x998,8);
  for(unsigned stage:{0u,0x45u,0x58u}){put(0xfa8,stage);if(covered(480,100,-32))throw std::runtime_error("Wrong phase");}
  put(0xfa8,0x44);put(0x10002+(6*128+34)*2,0x80);
  if(covered(480,100,-32) || !covered(480,84,-32))throw std::runtime_error("Wall destruction not followed by row");
  put(0x79b,0xa923);if(covered(480,84,-32))throw std::runtime_error("Other room");
  std::cout<<"PASS "<<count<<" bounds plus reveal/phase/pause/room guards\n";
}
