// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
template<class Read> bool smpCeresFightRoom(Read read) {
  return read(0x79b)==0xe0b5 && (read(0x7bb)==0xe0c7 || read(0x7bb)==0xe0e1) &&
    read(0x79f)==6 && read(0x7a7)==16 && (read(0x7bb)==0xe0e1 || read(0xf78)==0xe13f);
}
template<class Read> bool smpCeresArena(Read read) {
  return smpCeresFightRoom(read) && read(0x7a5)==32 && read(0x7a9)==2 &&
    (read(0x10002+(6*32+20)*2)&0xf000)==0x8000 &&
    (read(0x10002+(6*32+15)*2)&0xf000)==0;
}
template<class Read,class Write> void smpMoveCeresWallActor(Read read,Write write) {
  if(!smpCeresArena(read))return;
  // The Mode7 getaway substitutes an OBJ wall for the background wall.
  // Its native Ceres-door actor must follow the same physical relocation.
  for(unsigned slot=0;slot<0x800;slot+=64) {
    unsigned a=0xf78+slot;
    if(read(a)==0xe23f && read(a+60)==6 && read(a+2)==248)write(a+2,328);
  }
}
// The station's single-screen chamber needs a new row stride as well as five
// interior columns. Preserve its door, platforms, metatile/BTS pairs and PLMs.
template<class Read,class Write,class WriteByte> bool smpExpandCeresArena(Read read,Write write,WriteByte byte) {
  if(!smpCeresFightRoom(read) || read(0x7a5)!=16 || read(0x7a9)!=1 ||
     (read(0x10002+(6*16+15)*2)&0xf000)!=0x8000 ||
     (read(0x10002+(6*16+14)*2)&0xf000)!=0)return false;
  std::array<unsigned,256> blocks{},background{},bts{};
  for(unsigned n=0;n<256;n++){blocks[n]=read(0x10002+n*2);background[n]=read(0x19602+n*2);bts[n]=read(0x16402+n)&255;}
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<32;x++) {
    unsigned src=y*16+(x<15?x:x<20?14:15),dest=y*32+x;
    write(0x10002+dest*2,x<=20?blocks[src]:0x83ff);
    write(0x19602+dest*2,x<=20?background[src]:0x00ff);
    byte(0x16402+dest,x<=20?bts[src]:0);
  }
  for(unsigned n=0;n<80;n+=2)if(read(0x1c37+n)) {
    unsigned old=read(0x1c87+n)/2;if(old>=256)continue;
    write(0x1c87+n,((old/16)*32+old%16)*2);
  }
  write(0x10000,1024);write(0x7a5,32);write(0x7a9,2);
  byte(0xcd21,0); // Keep the native camera locked; the added area is visible.
  if(read(0xf78)==0xe13f && read(0x8006)==224)write(0x8006,304); // Ridley's own horizontal movement limit.
  return true;
}
