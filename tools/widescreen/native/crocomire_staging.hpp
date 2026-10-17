// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "terrain.hpp"
namespace SMPWide {
inline bool crocomireStaging(Bytes ram) {
  unsigned stage=word(ram,0xfa8);
  return word(ram,0x79b)==0xa98d && word(ram,0xf78)==0xddbf &&
    gameplayPresentationState(word(ram,0x998)) && stage>=0x3e && stage<=0x52 && !(stage&1);
}
}
