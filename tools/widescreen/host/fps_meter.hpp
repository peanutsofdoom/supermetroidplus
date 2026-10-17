// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
// Presentation-rate telemetry only. Caller supplies a monotonic wall clock;
// this value must never feed emulated clocks, scheduling or game state.
class SmpFpsMeter {
  bool started=false;
  uint64_t firstFrame=0,lastFrame=0;
  double firstTime=0,lastTime=0,value=-1;
public:
  void reset(){started=false;value=-1;}
  void sample(uint64_t frame,double now) {
    if(!started || frame<lastFrame || now<lastTime || now-lastTime>2.0) {
      started=true;firstFrame=lastFrame=frame;firstTime=lastTime=now;value=-1;return;
    }
    if(frame==lastFrame)return; // UI redraws are not additional game frames.
    lastFrame=frame;lastTime=now;
    if(now-firstTime>=1.0){value=double(frame-firstFrame)/(now-firstTime);firstFrame=frame;firstTime=now;}
  }
  double fps() const{return value;}
};
