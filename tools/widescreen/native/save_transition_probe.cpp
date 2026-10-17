// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_geometry.hpp"
#include <iostream>
#include <vector>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){
 std::vector<uint8_t> old(131072),next(131072),vram(65536),pal(512);
 for(auto* r:{&old,&next}){put(*r,0x7a5,16);put(*r,0x7a7,16);put(*r,0x78d,0x9000);put(*r,0x7bb,0xa237);}
 put(old,0x79b,0xa011);put(next,0x79b,0xa22a);put(next,0x998,11);
 for(unsigned dir:{0u,1u}){
  put(next,0x791,dir);put(next,0x927,0);put(next,0x929,0);
  for(int distance:{-1,0,1}){
   // The native viewport crosses the seam at camera zero for both directions.
   put(next,0x911,dir? -distance:distance);
   SceneGeometry g(next,next,old,vram,pal,0,true,0,0);
   auto departed=g.pixel(dir?256:-1,32);
   if(departed.valid!=(distance<0))throw std::runtime_error("horizontal completion boundary");
   if(!g.pixel(128,32).valid)throw std::runtime_error("destination terrain lost");
  }
 }
 // Vertical geometry still selects departing terrain on its side of the seam.
 put(next,0x791,2);put(next,0x929,16);put(next,0x911,0);
 SceneGeometry vertical(next,next,old,vram,pal,0,true,0,0);
 if(!vertical.pixel(16,8).valid)throw std::runtime_error("vertical path changed");
 std::cout<<"PASS both horizontal completion boundaries, destination terrain and vertical control\n";
}
