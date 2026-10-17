#include "raster_timeline.hpp"
#include <iostream>
using namespace SMPWide;
int main(){try{
 std::vector<uint8_t> ram(131072),vram(65536),pal(512);
 auto put=[&](unsigned a,unsigned v){ram[a]=v;ram[a+1]=v>>8;};
 put(0x998,8);put(0x79b,0x91f8);put(0x7bb,0x9261);put(0x7a5,144);put(0x7a7,80);put(0x943,1);put(0x1840,1);put(0x183e,6);put(0x911,600);
 auto test=[&](int x,int camera){return SceneGeometry(ram,ram,{},vram,pal,0,false,0,0).quakeEdgeCandidate(x,camera);};
 for(int shift=-8;shift<=8;shift++)for(int x=-43;x<299;x++){
  bool expected=(shift>0 && shift<=3 && x>=0 && x<shift)||(shift<0 && shift>=-3 && x>=256+shift && x<256);
  if(test(x,600-shift)!=expected)throw std::runtime_error("escape quake pixel scope mismatch");
 }
 for(auto change:std::vector<std::pair<unsigned,unsigned>>{{0x998,9},{0x79b,0x91f9},{0x7bb,0x9262},{0x943,0},{0x1840,0},{0x183e,5}}){auto original=ram;put(change.first,change.second);if(test(255,603)||test(0,597))throw std::runtime_error("escape quake scene guard failed");ram=original;}
 put(0x79b,0x92fd);put(0x7bb,0x9348);put(0x183e,24);
 for(int shift=-8;shift<=8;shift++)for(int x=-43;x<299;x++){
  bool expected=(shift>0 && shift<=3 && x>=0 && x<shift)||(shift<0 && shift>=-3 && x>=256+shift && x<256);
  if(test(x,600-shift)!=expected)throw std::runtime_error("Parlor escape quake pixel scope mismatch");
 }
 put(0x7bb,0x932e);if(test(0,597)||test(255,603))throw std::runtime_error("Parlor non-escape state changed");
 std::cout<<"PASS both escape quake directions, all pixel/shift limits, room/state/timer/game guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
