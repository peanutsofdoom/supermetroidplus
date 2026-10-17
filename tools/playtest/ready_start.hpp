// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <stdexcept>
namespace SMPPlaytest {
inline constexpr const char* ReadyProfile="saved-ship-v1";
inline constexpr const char* ReadySramSha="b55e8f09b5abe79127f3f896dc762ca6c963f3205dab0aaef6b0aa5bb3991a8f";
inline constexpr unsigned StartupFrameLimit=3600;
inline constexpr const char* ExplorationBudget="exploration-5m-v1";
inline constexpr unsigned ExplorationFrameLimit=18000;
struct ReadyState {
  unsigned game,room,roomState,currentHandler,newHandler,x,y,health,maxHealth,items,missiles,maxMissiles,movement,ySpeed,ySubspeed;
};
inline bool readyState(const ReadyState& s) {
  return s.game==8 && s.room==0x91f8 && s.roomState==0x922d &&
    s.currentHandler==0xe695 && s.newHandler==0xe725 &&
    s.x>=1144 && s.x<=1160 && s.y>=1080 && s.y<=1120 &&
    s.health==99 && s.maxHealth==99 && s.items==4 && s.missiles==5 && s.maxMissiles==5 &&
    s.movement<=1 && !s.ySpeed && !s.ySubspeed;
}
inline unsigned startupButtons(uint64_t frame,unsigned game) {
  // Only menu states receive pulses; loading and gameplay always receive neutral.
  return (game==1 || game==2 || game==4 || game==5) && frame%30<2 ? 0x1080 : 0;
}
inline int16_t startupPoll(unsigned port,unsigned input,unsigned buttons) {
  if(port>1 || input>=12)throw std::runtime_error("Unsupported startup controller input");
  static constexpr unsigned bits[]={11,10,9,8,15,7,14,6,5,4,13,12};
  return port==0 ? int16_t((buttons>>bits[input])&1) : 0;
}
}
