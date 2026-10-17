// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Experimental room-independent policy. The renderer still validates each
// captured scene, memory image and raster. This is not a qualification gate.
class SMPPlaythroughEligibility {
  bool prior=false;
public:
  void reset(){prior=false;}
  bool allow(unsigned state,unsigned phase,unsigned room,unsigned) {
    // State7 is the visible room fade after loading finishes, before control
    // returns to gameplay. Observe it while still black to prime ship history.
    // 26h is the native gameplay fade after ship takeoff, before the ending.
    if(state==7 || state==8 || state==0x0c || state==0x12 || (state>=0x13 && state<=0x19) || state==0x1b || state==0x26 || state==0x29 || state==0x2a){prior=room>=0x8000 && room<=0xffff;return prior;}
    if(!prior)return false;
    if(state==9)return true;
    if(state!=11)return false;
    switch(phase) {
      case 0xe17d:case 0xe29e:case 0xe2db:case 0xe2f7:case 0xe310:
      case 0xe353:case 0xe36e:case 0xe38e:case 0xe3c0:case 0xe4a9:
      case 0xe659:case 0xe664:case 0xe6a2:case 0xe737:return true;
      default:return false;
    }
  }
};
