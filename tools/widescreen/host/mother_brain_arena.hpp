// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
// Owner-requested arena expansion. Five new metatile columns continue the
// chamber's central architecture; its original right wall moves 80px right.
// The live level/BTS cells remain the collision authority, including PLMs.
template<class Read> bool smpMotherBrainRoom(Read read) {
  return read(0x79b)==0xdd58 && read(0x7bb)==0xdd6e && read(0x7a5)==64 && read(0x7a7)==16 &&
    read(0xf78)==0xec7f && read(0xfb8)==0xec3f;
}
template<class Read> bool smpMotherBrainArena(Read read) {
  return smpMotherBrainRoom(read) && (read(0x10002+(6*64+20)*2)&0xf000)==0x8000 &&
    (read(0x10002+(6*64+15)*2)&0xf000)==0;
}
template<class Read,class Write,class WriteByte> bool smpExpandMotherBrainArena(Read read,Write write,WriteByte writeByte) {
  if(!smpMotherBrainRoom(read) || read(0x10002+(6*64+15)*2)!=0x830f ||
     read(0x10002+(6*64+20)*2)!=0x00ff)return false;
  std::array<unsigned,16*10> blocks{},bts{};
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<10;x++) {
    unsigned source=y*64+x+6;
    blocks[y*10+x]=read(0x10002+source*2);bts[y*10+x]=read(0x16402+source)&255;
  }
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<10;x++) {
    unsigned dest=y*64+x+11;
    write(0x10002+dest*2,blocks[y*10+x]);writeByte(0x16402+dest,bts[y*10+x]);
  }
  // Sealing PLMs are born in the original wall and must finish in the moved
  // wall. Other room PLMs retain their original coordinates and instructions.
  for(unsigned n=0;n<80;n+=2)if(read(0x1c37+n)==0xb673) {
    unsigned block=read(0x1c87+n);
    if((block/2)%64==15)write(0x1c87+n,block+10);
  }
  return true;
}
