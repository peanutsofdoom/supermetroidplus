// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
class SMPGuiEligibility {
  bool prior=false;
  unsigned priorRoom=0,priorState=0;
  static bool room(unsigned r){return r==0x91f8 || r==0x92fd || r==0x96ba || r==0x975c || r==0x97b5 || r==0x9e9f || r==0x9f11 || r==0xa107;}
  static bool route(unsigned from,unsigned fromState,unsigned to,unsigned toState=0) {
    if(!profile(from,fromState))return false;
    // Same-room headers are allowed during preload, not as an untested reload.
    if(from==to)return !toState;
    struct Edge {unsigned from,state,to,next;};
    static constexpr Edge edges[]={
      {0x91f8,0x9213,0x92fd,0x9314},{0x92fd,0x9314,0x91f8,0x9213},
      {0x92fd,0x9314,0x96ba,0x96d1},{0x96ba,0x96d1,0x92fd,0x9314},
      {0x96ba,0x96d1,0x975c,0x976d},{0x975c,0x976d,0x97b5,0x97c6},
      {0x97b5,0x97c6,0x9e9f,0x9eb1},{0x9e9f,0x9eb1,0x9f11,0x9f23},
      {0x9f11,0x9f23,0xa107,0xa114},{0xa107,0xa114,0x9f11,0x9f23},
      {0x9f11,0x9f23,0x9e9f,0x9eb1},
      {0x97b5,0x97e0,0x975c,0x9787},
      {0x9e9f,0x9eb1,0x97b5,0x97e0},{0x975c,0x9787,0x96ba,0x96eb}};
    for(const auto& edge:edges)
      if(edge.from==from && edge.state==fromState && edge.to==to && (!toState || edge.next==toState))return true;
    return false;
  }
  static bool profile(unsigned r,unsigned s) {
    // Awakened Landing Site has identical rendering headers; its door edges
    // remain separately qualified by the explicit route table above.
    return (r==0x91f8 && (s==0x9213 || s==0x922d)) || (r==0x92fd && s==0x9314) || (r==0x96ba && (s==0x96d1 || s==0x96eb)) ||
      (r==0x975c && (s==0x976d || s==0x9787)) || (r==0x97b5 && (s==0x97c6 || s==0x97e0)) || (r==0x9e9f && s==0x9eb1) ||
      (r==0x9f11 && s==0x9f23) || (r==0xa107 && s==0xa114);
  }
public:
  static bool qualifiedTransition(unsigned from,unsigned fromState,unsigned to,unsigned toState) {
    return profile(to,toState) && route(from,fromState,to,toState);
  }
  void reset(){prior=false;priorRoom=priorState=0;}
  bool allow(unsigned state,unsigned phase,unsigned r,unsigned s) {
    if(state==8){prior=profile(r,s);priorRoom=prior?r:0;priorState=prior?s:0;return prior;}
    if(!prior || !room(r) || !route(priorRoom,priorState,r))return false;
    if(state==9)return true;
    if(state!=11)return false;
    // During loading the room pointer changes before its state/assets finish.
    if(phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7 ||
       phase==0xe310 || phase==0xe353 || phase==0xe36e)return true;
    if(phase==0xe38e || phase==0xe3c0 || phase==0xe4a9 || phase==0xe659 ||
       phase==0xe664 || phase==0xe6a2 || phase==0xe737)return qualifiedTransition(priorRoom,priorState,r,s);
    return false;
  }
};
