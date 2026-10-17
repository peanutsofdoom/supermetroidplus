// SPDX-License-Identifier: GPL-3.0-or-later
// Selection uses completed upload-source snapshots, never writable guest RAM.
#pragma once
#include "terrain.hpp"
#include <array>
#include <memory>
#include <optional>
#include <vector>
namespace SMPWide {
struct OwnerImage {
  const uint64_t tag;
  const std::vector<uint8_t> bytes;
  OwnerImage(uint64_t identity,Bytes source):tag(identity),bytes(source.begin(),source.end()) {
    if(bytes.size()!=131072)throw std::invalid_argument("incomplete owner image");
  }
};
using OwnerRef=std::shared_ptr<const OwnerImage>;
enum class SceneKind {Ordinary,Staging,ReadyBeforeScroll,Right,Left,Down,Up};
struct SceneSelection {
  OwnerRef assets,view,previous;
  SceneKind kind;
  int exitX=-1,exitY=-1;
};
class SceneSelector {
  OwnerRef active,previous;
  std::optional<std::array<uint16_t,4>> destination;
  int exitX=0,exitY=0;
  static bool preload(unsigned p) {
    return p==0xe17d || p==0xe29e || p==0xe2db || p==0xe2f7 || p==0xe310 || p==0xe353 || p==0xe36e;
  }
  static bool scrolling(unsigned p) {
    return p==0xe3c0 || p==0xe4a9 || p==0xe659 || p==0xe664 || p==0xe6a2 || p==0xe737;
  }
  static std::array<uint16_t,4> context(Bytes ram) {
    return {word(ram,0x79b),word(ram,0x7bb),word(ram,0x78d),uint16_t(word(ram,0x791)&3)};
  }
public:
  void reset() {active.reset();previous.reset();destination.reset();exitX=exitY=0;}
  SceneSelection select(OwnerRef owner) {
    if(!owner)throw std::invalid_argument("missing owner image");
    Bytes ram=owner->bytes;
    auto state=word(ram,0x998),phase=word(ram,0x99c);
    if(gameplayPresentationState(state) || (state>=0x15 && state<=0x19)) {
      active=owner;previous.reset();destination.reset();
      return {owner,owner,{},SceneKind::Ordinary};
    }
    if(!active)throw std::invalid_argument("transition has no prior scene");
    if(state==9 || (state==11 && preload(phase))) {
      bool beforeHeader=state==9 || phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7;
      if(beforeHeader && word(ram,0x79b)==word(active->bytes,0x79b))active=owner;
      return {active,owner,{},SceneKind::Staging};
    }
    if(state==11 && phase==0xe38e) {
      previous=active;exitX=word(ram,0x911);exitY=word(ram,0x915);destination=context(ram);
      return {previous,owner,{},SceneKind::ReadyBeforeScroll};
    }
    if(state==11 && scrolling(phase)) {
      if(!previous || !destination || *destination!=context(ram))
        throw std::invalid_argument("scrolling lacks matching completed setup");
      return {owner,owner,previous,SceneKind(3+(*destination)[3]),exitX,exitY};
    }
    throw std::invalid_argument("unqualified scene state or phase");
  }
};
}
