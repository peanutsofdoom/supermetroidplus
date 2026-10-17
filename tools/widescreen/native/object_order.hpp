// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Group zero is reserved for B4 sprite effects, drawn at A0:8855.
// Retail A0:8859..887C: explosions, high enemy projectiles, layers0..2,
// Samus/projectiles, layers3..5, low enemy projectiles, layers6..7.
inline unsigned smpObjectGroup(unsigned group){return (group+1)<<20;}
inline unsigned smpEnemyOrder(unsigned layer,unsigned slot,unsigned entry) {
  static constexpr unsigned groups[]={2,3,4,6,7,8,10,11};
  return smpObjectGroup(groups[layer])+slot*64+entry;
}
