#include "eye_state.hpp"
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
int main(){try{std::vector<uint8_t> ram(131072),table(256);put(ram,0x79b,0x9e9f);put(ram,0x7bb,0x9eb1);put(ram,0x9000,1);put(ram,0x592,0x8000);EyeState e;e.configure(table);e.owner(ram);e.write(0x9100,255,1);ram[0x9100]=255;e.owner(ram);std::cout<<"PASS Power Bomb table writes are not parsed as eye beam angles\n";}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
