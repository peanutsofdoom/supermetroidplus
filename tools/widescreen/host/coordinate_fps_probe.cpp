#include "coordinate_overlay.hpp"
#include <cassert>
#include <iostream>
int main(){
  for(auto size:{std::pair{256u,240u},std::pair{512u,480u},std::pair{2048u,448u}}){
    auto [w,h]=size;std::vector<uint32_t> input(w*h),copied,inplace;
    for(unsigned i=0;i<input.size();i++)input[i]=i*7919;
    auto original=input;inplace=input;
    std::string first="D64 R:91F8 S:9247 G:08 W F:003538",second="P:00917,01163 C:00746,01024 FPS:55.3";
    smpCoordinateSurface(input.data(),w*4,w,h,0,first,second,copied);
    smpDrawCoordinatePanel(inplace.data(),w,h,0,first,second);
    assert(input==original && inplace==copied && inplace!=input);
    assert((second.size()*6+7)*(w==2048?6:w>=512?2:1)<w);
  }
  auto dot=smpCoordinateGlyph('.');assert(dot[1] && dot[2]);
  std::cout<<"PASS FPS panel fits native/wide layouts, decimal glyph, copy/in-place parity and native buffer preservation\n";
}
