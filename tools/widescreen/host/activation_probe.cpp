#include "experimental_activation.hpp"
#include <stdexcept>
#include <iostream>
int main(){
  for(unsigned camera:{0u,43u,256u,4096u,16000u})for(int relative=-100;relative<400;relative++) {
    unsigned x=uint16_t(camera+relative),radius=8;
    bool left=smpExpandedActivationNegative(uint16_t(x+radius),camera);
    bool right=smpExpandedActivationNegative(uint16_t(camera+256+radius),x);
    if((!left && !right)!=(relative+8>=-43 && relative-8<=299))throw std::runtime_error("activation boundary differs");
  }
  std::cout<<"PASS: expanded left/right activation boundaries, room-edge wrapping and hitbox radius\n";
}
