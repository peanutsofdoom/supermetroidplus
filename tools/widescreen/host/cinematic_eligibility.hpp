// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// 1: Zebes ending. 2: Ceres exterior / flight to Zebes. 3: Ceres Mode7 room.
// Intro story text and flashback scenes retain their existing presentation.
inline unsigned smpCinematicScene(unsigned game,unsigned function,unsigned room,unsigned bgMode) {
  if(game==0x27)return 1;
  if(game==0x22 || game==0x25)return 2;
  if(game==0x1e) {
    switch(function) {
      case 0xbca0:case 0xbde4:case 0xbdf9:case 0xbfda:case 0xa38f:case 0xc0c5:case 0xc100:return 2;
    }
  }
  bool station=room==0xdf45 || room==0xdf8d || room==0xdfd7 || room==0xe021 || room==0xe06b || room==0xe0b5;
  bool playing=game==7 || game==8 || game==9 || game==11 || game==0x20 || game==0x21 || game==0x23 || game==0x24;
  // The elevator's door ASM switches to Mode7 within the arriving frame.
  if(room==0xdf45 && game==11)return 3;
  return station && playing && (bgMode&7)==7?3:0;
}
