// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
// Owner-approved gameplay divergence. These two BMI decisions delimit the
// horizontal active-enemy list at retail A0:8F0A and A0:8F1A. All other flags,
// vertical checks, per-enemy flags, AI routines and collision code are retained.
inline bool smpExpandedActivationNegative(unsigned lhs,unsigned rhs) {
  return (uint16_t(lhs-rhs+43)&0x8000)!=0;
}
// Owner-approved Brinstar pirate laser lifetime extension. Only the two
// horizontal decisions in the retail projectile bounds routine are changed.
inline bool smpPirateLaserBoundsNegative(unsigned pc,unsigned x,unsigned camera) {
  return (uint16_t(x-camera+(pc==0x86e6e8?43:-299))&0x8000)!=0;
}

// Owner-approved Cacatac spike bounds: native DAD4 compares x-camera,
// while DAE0 compares camera+256-x (opposite operand order).
inline bool smpCacatacSpikeBoundsNegative(unsigned pc,unsigned x,unsigned camera) {
  return (uint16_t(pc==0x86dad4?x-camera+43:camera+298-x)&0x8000)!=0;
}

// Owner-approved F193 pipe-bug flight reset extension. Retain the original
// absolute-negative-x check and native return path; only branch N is changed.
inline bool smpPipeBugBoundsNegative(unsigned pc,unsigned x,unsigned camera,unsigned radius) {
  int relative=int(int16_t(uint16_t(x-camera)));
  return pc==0xa0c19b ? relative+int(radius)+43<0 : relative-int(radius)-299<0;
}

// Owner-approved B7 Zoa flight-reset boundary. Only horizontal branch N is
// changed; the native vertical test, spawning and movement stay in the ROM.
inline bool smpZoaBoundsNegative(unsigned pc,unsigned x,unsigned camera,unsigned radius) {
  int relative=int(int16_t(uint16_t(x-camera)));
  return pc==0xa0ad79 ? relative+int(radius)<-43 : relative-int(radius)>=299;
}

// Owner-approved Crocomire ending barrier translation: [768,816) -> [811,859).
// Keep the native PLM and its lifetime intact; transform collision sampling,
// not level graphics or the event sequence. All coordinates are room pixels.
template<class ReadWord> inline bool smpCrocomireBarrierActive(ReadWord word) {
  unsigned stage=word(0xfa8),game=word(0x998);
  return (game==8 || game==0x0c || game==0x12) && word(0x79b)==0xa98d && word(0xf78)==0xddbf &&
    word(0x7a5)==128 && stage>=0x3e && stage<=0x48 && !(stage&1) &&
    word(0x10002+(3*128+48)*2)==0x8080 && word(0x10002+(4*128+48)*2)==0x8107;
}
inline bool smpCrocomireBarrierCell(unsigned block) {
  return block/128>=3 && block/128<=10 && block%128>=48 && block%128<=53;
}
inline bool smpCrocomireBarrierOverlap(unsigned block,int left,int right) {
  int x=int(block%128)*16;
  int lo=left>x?left:x,hi=right<x+16?right:x+16;
  if(lo<811)lo=811;if(hi>859)hi=859;
  return smpCrocomireBarrierCell(block) && lo<hi;
}
inline unsigned smpCrocomireBarrierType(unsigned block,unsigned tile,int left,int right) {
  if(!smpCrocomireBarrierCell(block))return tile;
  if(smpCrocomireBarrierOverlap(block,left,right))return (tile&0xfff)|0x8000;
  if(block%128<=50 && (tile&0xf000)==0x8000)return tile&0xfff;
  return tile;
}

// Owner-approved left-stage relocation. Keep the native wall creation/removal
// events and spike BTS; translate its collision sampling by the same43pixels
// as the wall artwork. The later invisible left barrier follows it too.
struct SmpCrocomireLeftWall {int left=0,right=0;bool spikes=false;explicit operator bool() const{return right>left;}};
template<class ReadWord> inline SmpCrocomireLeftWall smpCrocomireLeftWall(ReadWord word,unsigned block) {
  unsigned stage=word(0xfa8),game=word(0x998),row=block/128,col=block%128;
  if((game!=8 && game!=12 && game!=18) || word(0x79b)!=0xa98d || word(0xf78)!=0xddbf ||
     word(0x7a5)!=128 || stage<0x3e || stage>0x48 || (stage&1) || row<3 || row>10 || col<27 || col>34)return {};
  unsigned base=0x10002+row*256;
  if(word(base+68)==0xa49c && (word(base+64)==0x8140 || word(base+64)==0x8120))return {469,517,true};
  if((word(base+60)&0xf000)==0x8000 && (word(base+64)&0xf000)==0x8000 && (word(base+66)&0xf000)==0)return {437,485,false};
  return {};
}
inline bool smpCrocomireLeftOverlap(SmpCrocomireLeftWall w,unsigned block,int left,int right) {
  int x=int(block%128)*16;
  int lo=left>x?left:x,hi=right<x+16?right:x+16;
  if(lo<w.left)lo=w.left;if(hi>w.right)hi=w.right;
  return bool(w) && lo<hi;
}
inline unsigned smpCrocomireLeftType(SmpCrocomireLeftWall w,unsigned block,unsigned tile,int left,int right) {
  if(!w)return tile;
  if(smpCrocomireLeftOverlap(w,block,left,right)) {
    int lo=left>int(block%128)*16?left:int(block%128)*16;
    int hi=right<int(block%128)*16+16?right:int(block%128)*16+16;
    return (tile&0xfff)|(w.spikes && lo<517 && hi>501?0xa000:0x8000);
  }
  int x=int(block%128)*16;
  if(x>=w.left+43 && x<w.right+43)return tile&0xfff;
  return tile;
}
