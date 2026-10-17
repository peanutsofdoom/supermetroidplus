// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_timeline.hpp"
#include <cassert>
#include <iostream>
using namespace SMPWide;
int main() {
  std::vector<uint8_t> ram(131072),v(65536),p(512),r(205);
  auto owner=std::make_shared<const OwnerImage>(1,ram);
  auto vr=memoryImage(v,v.size()),pal=memoryImage(p,p.size()),regs=memoryImage(r,r.size());
  SpriteContextCache cache;
  auto first=cache.get(owner,{},vr,pal,regs,{},{});
  auto put=[&](Reg k,unsigned value){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)r[f.offset+i]=value>>(8*i);};
  put(Reg::hoffset,100);put(Reg::vcounter,70);
  auto scrolling=memoryImage(r,r.size());
  assert(cache.get(owner,{},vr,pal,scrolling,{},{})==first);
  for(auto k:{Reg::displayDisable,Reg::obj_baseSize,Reg::obj_interlace,Reg::obj_firstSprite,Reg::obj_tiledataAddress,
      Reg::obj_nameselect,Reg::obj_priority0,Reg::obj_priority1,Reg::obj_priority2,Reg::obj_priority3}) {
    auto before=cache.get(owner,{},vr,pal,memoryImage(r,r.size()),{},{});
    put(k,1);assert(cache.get(owner,{},vr,pal,memoryImage(r,r.size()),{},{})!=before);put(k,0);
  }
  auto current=cache.get(owner,{},vr,pal,regs,{},{});
  auto changedV=memoryImage(v,v.size());assert(cache.get(owner,{},changedV,pal,regs,{},{})!=current);
  current=cache.get(owner,{},vr,pal,regs,{},{});
  assert(cache.get(owner,{},vr,memoryImage(p,p.size()),regs,{},{})!=current);
  current=cache.get(owner,{},vr,pal,regs,{},{});
  auto otherOwner=std::make_shared<const OwnerImage>(2,ram);
  assert(cache.get(otherOwner,{},vr,pal,regs,{},{})!=current);
  current=cache.get(owner,{},vr,pal,regs,{},{});
  assert(cache.get(owner,otherOwner,vr,pal,regs,{},{})!=current);
  // Cache replacement must not invalidate an earlier row's borrowed images.
  assert(first->registers==regs && first->view==owner && first->vram==vr);
  std::cout<<"PASS sprite cache reuse, OBJ/image/owner/history invalidation and retained image lifetime\n";
}
