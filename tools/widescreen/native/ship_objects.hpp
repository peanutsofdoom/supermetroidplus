// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded passive gunship descriptor and OBJ sampler. No guest mutation.
#pragma once
#include "registers.hpp"
#include "object_order.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <vector>
namespace SMPWide {
struct ShipPixel {unsigned color,priority,palette,order=0;};
struct ShipPiece {unsigned enemy,entry,size,attributes,oam;int x,y;bool marginOnly=false;};
inline int shipSigned(unsigned v,unsigned bits) {
  v&=(1u<<bits)-1;return v&(1u<<(bits-1))?int(v)-int(1u<<bits):int(v);
}
class ShipSampler {
  std::vector<ShipPiece> parts;
  std::array<std::pair<size_t,size_t>,224> rowRanges{};
  int minX=32767,maxX=-32768;
  Bytes vram,palette;
  Registers registers;
public:
  // ROM bytes A2:AD81..AFF2, supplied only after retail ROM identity check.
  ShipSampler(Bytes ram,Bytes maps,Bytes oam,Bytes v,Bytes c,Bytes r,Bytes previous={}):
    vram(v),palette(c),registers(r) {
    if(ram.size()!=131072 || maps.size()!=0x272 || oam.size()!=544 || v.size()!=65536 || c.size()!=512)
      throw std::invalid_argument("ship image sizes");
    if(word(ram,0x79b)!=0x91f8 || registers.get(Reg::obj_baseSize) ||
       registers.get(Reg::obj_interlace) || registers.get(Reg::obj_firstSprite))
      throw std::invalid_argument("unqualified ship configuration");
    unsigned slots=0;
    for(unsigned slot=0;slot<0x800;slot+=64) {
      unsigned a=0xf78+slot,id=word(ram,a);
      if(id!=0xd07f && id!=0xd0bf)continue;
      if(slot>128 || word(ram,a+46)!=0xa2 || (word(ram,a+16)&4) ||
         word(ram,a+36) || word(ram,a+38) || word(ram,a+42))
        throw std::invalid_argument("unqualified ship state");
      slots|=1u<<(slot/64);
      unsigned pointer=word(ram,a+22);
      // Retail A2:804D is the zero-entry common spritemap. Room loading sets
      // instruction-driven parts to it before their first native AI update.
      // It contributes no descriptors; never infer an unexecuted ship pose.
      if(pointer==0x804d)continue;
      if(pointer<0xad81 || pointer>0xaff1)throw std::invalid_argument("ship map pointer");
      unsigned pos=pointer-0xad81,count=word(maps,pos);
      if(!count || count>34 || pos+2+count*5>maps.size())throw std::invalid_argument("ship map bounds");
      int ox=shipSigned(word(ram,a+2)-word(ram,0x911)+word(ram,0x7010+slot),16);
      int oy=shipSigned(word(ram,a+6)-word(ram,0x915)+word(ram,0x7012+slot),16);
      for(unsigned n=0;n<count;n++) {
        unsigned p=pos+2+n*5,raw=word(maps,p);
        ShipPiece q{slot,n,raw&0x8000?16u:8u,
          ((unsigned(word(maps,p+3))+word(ram,a+32))&65535)|word(ram,a+30),0,
          ox+shipSigned(raw,9),oy+shipSigned(maps[p+2],8)};
        parts.push_back(q);
      }
    }
    if(slots!=7)throw std::invalid_argument("missing ship part");
    auto relevant=[](const auto& q){return q.x+int(q.size)>-43 && q.x<299 && q.y+int(q.size)>0 && q.y<224;};
    if(!std::any_of(parts.begin(),parts.end(),relevant)){parts.clear();return;}
    if(!previous.empty() && (previous.size()!=131072 || word(previous,0x79b)!=0x91f8))
      throw std::invalid_argument("invalid previous ship camera owner");
    auto all=std::move(parts);parts.clear();int lastIndex=-1;
    for(unsigned slot:{0u,64u,128u}) {
      if(!std::any_of(all.begin(),all.end(),[&](auto& q){return q.enemy==slot && relevant(q);}))continue;
      unsigned slotAddress=0xf78+slot;
      if(word(ram,slotAddress+14)!=0x2400 || word(ram,slotAddress+34)!=2 || (word(ram,0x17a2)&1))
        throw std::invalid_argument("unqualified ship drawing suppression");
      // During the recorded running Screw Attack, native OAM sometimes keeps
      // a complete ship group's X/tile/order but explicitly hides every Y at
      // F0. Respect that native blanking instead of rejecting the whole frame.
      // Require the complete ordered group; ordinary offscreen culling still
      // follows the previous-camera proof below.
      bool blanked=true;int blankIndex=lastIndex;
      for(const auto& q:all)if(q.enemy==slot) {
        bool match=false;
        for(unsigned i=unsigned(blankIndex+1);i<128;i++) {
          unsigned high=oam[512+i/4]>>(i%4*2);
          if(oam[i*4+1]==240 && (unsigned(q.x)&511)==(oam[i*4]|((high&1)<<8)) &&
             q.attributes==word(oam,i*4+2) && q.size==(high&2?16u:8u)) {
            blankIndex=int(i);match=true;break;
          }
        }
        if(!match){blanked=false;break;}
      }
      if(blanked){lastIndex=blankIndex;continue;}
      unsigned count=0,found=0;std::vector<unsigned> indices;
      for(auto& q:all)if(q.enemy==slot) {
        count++;
        unsigned matches=0;
        for(unsigned i=0;i<128;i++) {
          unsigned high=oam[512+i/4]>>(i%4*2);
          if((unsigned(q.x)&511)==(oam[i*4]|((high&1)<<8)) &&
             (unsigned(q.y)&255)==oam[i*4+1] && q.attributes==word(oam,i*4+2) &&
             q.size==(high&2?16u:8u)){q.oam=i;matches++;}
        }
        if(matches>1)throw std::invalid_argument("ship piece lacks unique OAM witness");
        if(matches){found++;indices.push_back(q.oam);}
      }
      if(found && found!=count)throw std::invalid_argument("ship piece lacks unique OAM witness");
      unsigned a=0xf78+slot;
      if(word(ram,a+14)!=0x2400 || word(ram,a+34)!=2 || (word(ram,0x17a2)&1))
        throw std::invalid_argument("unqualified ship drawing suppression");
      bool vertical=false;
      if(!previous.empty()) {
        int x=shipSigned(word(ram,a+2)-word(previous,0x911),16);
        int y=shipSigned(word(ram,a+6)-word(previous,0x915),16),radius=word(ram,a+10);
        bool horizontal=x+radius<0 || x-radius>256;
        vertical=y+8<0 || y>248;
        if((found==0)!=(horizontal || vertical))throw std::invalid_argument("ship draw phase differs from OAM");
      } else if(!found)throw std::invalid_argument("missing ship camera history");
      for(auto index:indices) {
        if(int(index)<=lastIndex)throw std::invalid_argument("ship OAM order differs");
        lastIndex=int(index);
      }
      if(vertical)continue;
      for(auto q:all)if(q.enemy==slot && relevant(q)) {
        // Previous-owner culling can suppress a group whose current-camera
        // rectangle crosses the native seam. Reconstruct only outside it;
        // even opaque pixels omitted by the native game must stay omitted.
        q.marginOnly=!found;
        q.oam=slot+q.entry;parts.push_back(q);
      }
    }
    std::sort(parts.begin(),parts.end(),[](auto& a,auto& b){return a.oam<b.oam;});
    for(auto& range:rowRanges)range={parts.size(),0};
    for(size_t i=0;i<parts.size();i++) {
      const auto& q=parts[i];
      minX=std::min(minX,q.x);maxX=std::max(maxX,q.x+int(q.size));
      for(int y=std::max(0,q.y);y<std::min(224,q.y+int(q.size));y++) {
        rowRanges[y].first=std::min(rowRanges[y].first,i);
        rowRanges[y].second=i+1;
      }
    }
  }
  const auto& descriptors() const {return parts;}
  std::optional<ShipPixel> pixel(int x,unsigned line) const {
    if(x<minX || x>=maxX)return {};
    auto range=line>=1 && line<=224?rowRanges[line-1]:std::pair<size_t,size_t>{0,parts.size()};
    for(size_t i=range.first;i<range.second;i++) {
      const auto& q=parts[i];
      if(q.marginOnly && x>=0 && x<256)continue;
      int dx=x-q.x,dy=int(line)-1-q.y;
      if(dx<0 || dy<0 || dx>=int(q.size) || dy>=int(q.size))continue;
      auto attr=q.attributes;
      if(attr&0x4000)dx=int(q.size)-1-dx;
      if(attr&0x8000)dy=int(q.size)-1-dy;
      unsigned base=registers.get(Reg::obj_tiledataAddress);
      if(attr&256)base+=(1+registers.get(Reg::obj_nameselect))<<12;
      unsigned tile=(((attr&255)/16+unsigned(dy)/8)&15)*16+((attr+unsigned(dx)/8)&15);
      unsigned address=((base+tile*16)&32767)*2,index=0;
      for(unsigned bit=0;bit<4;bit++)index|=((vram[(address+(bit/2)*16+(dy%8)*2+bit%2)&65535]>>(7-dx%8))&1)<<bit;
      if(!index)continue;
      unsigned pal=(attr>>9)&7;
      return ShipPixel{unsigned(word(palette,(128+pal*16+index)*2)&32767),
        registers.get(Reg(unsigned(Reg::obj_priority0)+((attr>>12)&3))),pal,smpEnemyOrder(2,q.enemy,q.entry)};
    }
    return {};
  }
};
}
