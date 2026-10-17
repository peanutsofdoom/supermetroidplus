// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental passive enemy margins. Retail A0:944A and 81:8AB8/8B22 formats.
#pragma once
#include "ship_objects.hpp"
#include "crocomire_staging.hpp"
#include "../host/mother_brain_arena.hpp"
#include "../host/ceres_arena.hpp"
#include <array>
namespace SMPWide {
class EnemySampler {
  struct Piece {int x,y;unsigned size,attr,order,sortOrder=0;bool centerFill=false,stageShift=false;};
  std::vector<Piece> pieces,referencePieces;
  bool staging=false,arenaNative=false,centerFill=false;
  int arenaNativeLeft=236;
  std::vector<std::array<uint8_t,128>> arenaNativeRows;
  std::array<uint8_t,224> arenaNativeCounts{};
  std::array<bool,128> hiddenNativeObjects{},ceresTimerObjects{};
  std::array<std::pair<int,int>,224> ceresTimerBounds;
  std::array<std::pair<int,int>,224> stagingBounds;
  std::array<std::pair<size_t,size_t>,224> rowRanges{};
  int minX=32767,maxX=-32768;
  Bytes vram,palette,nativeOam;
  Registers registers;
  static unsigned romAddress(unsigned bank,unsigned pointer) {
    return (bank&127)*32768+(pointer&32767);
  }
  void indexRows() {
    for(auto& range:rowRanges)range={pieces.size(),0};
    for(size_t i=0;i<pieces.size();i++) {
      const auto& q=pieces[i];
      centerFill|=q.centerFill && q.x<256 && q.x+int(q.size)>0;
      minX=std::min(minX,q.x);maxX=std::max(maxX,q.x+int(q.size));
      for(int y=std::max(0,q.y);y<std::min(224,q.y+int(q.size));y++) {
        rowRanges[y].first=std::min(rowRanges[y].first,i);rowRanges[y].second=i+1;
      }
    }
  }
public:
  EnemySampler(Bytes ram,Bytes rom,Bytes v,Bytes c,Bytes r):vram(v),palette(c),nativeOam(ram.size()==131072?ram.subspan(0x370,544):Bytes{}),registers(r) {
    if(ram.size()!=131072 || rom.size()!=3145728 || v.size()!=65536 || c.size()!=512)
      throw std::invalid_argument("enemy image sizes");
    unsigned game=word(ram,0x998),phase=word(ram,0x99c);
    ceresTimerBounds.fill({32767,-32768});
    const bool motherArena=smpMotherBrainArena([&](unsigned a){return word(ram,a);});
    const bool ceresArena=smpCeresArena([&](unsigned a){return word(ram,a);});
    const bool wideArena=motherArena || ceresArena;
    if(ceresArena && word(ram,0x943))for(unsigned i=0;i<128;i++) {
      unsigned hi=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
      int x=ram[0x370+i*4]|((hi&1)<<8),y=ram[0x371+i*4];
      // 80:9FE8..A060: actual timer digits/TIME punctuation in native OAM.
      ceresTimerObjects[i]=!(hi&2) && (attr&0xfe00)==0x3a00 && (attr&511)>=0x1e0 &&
        (attr&511)<=0x1f8 && x>=int(ram[0x949])-32 && x<int(ram[0x949])+32 &&
        y>=int(ram[0x94b])-16 && y<int(ram[0x94b])+8;
      if(ceresTimerObjects[i])for(int row=std::max(0,y);row<std::min(224,y+8);row++) {
        ceresTimerBounds[row].first=std::min(ceresTimerBounds[row].first,x);
        ceresTimerBounds[row].second=std::max(ceresTimerBounds[row].second,x+8);
      }
    }
    const bool motherDeath=wideArena && game>=0x15 && game<=0x19;
    const int rightEdge=wideArena?342:299;
    bool kihunterDeparture=word(ram,0x79b)==0x948c && word(ram,0x7bb)==0x9499 &&
      (game==9 || (game==11 && (phase==0xe17d || phase==0xe29e || phase==0xe2db || phase==0xe2f7)));
    if((!gameplayPresentationState(game) && !kihunterDeparture && !motherDeath && !(ceresArena && game==11 && word(ram,0x911)==0)) || registers.get(Reg::obj_baseSize) ||
       registers.get(Reg::obj_interlace) || registers.get(Reg::obj_firstSprite))return;
    arenaNative=wideArena && !motherDeath;
    arenaNativeLeft=motherArena?176:236;
    if(arenaNative) {
      // Index the native pieces once per cached sprite context, not128times
      // for every new margin pixel. OAM order is retained within each row.
      arenaNativeRows.resize(224);
      for(unsigned i=0;i<128;i++) {
        unsigned hi=ram[0x570+i/4]>>(i%4*2),size=hi&2?16:8,y=ram[0x371+i*4];
        for(unsigned row=std::max(31u,y);row<std::min(224u,y+size);row++)
          arenaNativeRows[row][arenaNativeCounts[row]++]=uint8_t(i);
      }
    }
    if(motherDeath) {
      // The native death animation retains Samus's screen position. When she
      // dies in the expanded floor area, keep its actual OAM fragments visible
      // there too, including the armor burst's different tiles and palette.
      for(unsigned i=0;i<128;i++) {
        unsigned hi=ram[0x570+i/4]>>(i%4*2),size=hi&2?16:8;
        int x=ram[0x370+i*4]|((hi&1)<<8),y=ram[0x371+i*4];if(x>=342)x-=512;
        if(y>=224 || x+int(size)<=0 || x>=342 || (x>=0 && x+int(size)<=256))continue;
        pieces.push_back({x,y,size,word(ram,0x372+i*4),i,i});
      }
      indexRows();return;
    }
    staging=crocomireStaging(ram);
    for(auto& b:stagingBounds)b={32767,-32768};
    for(unsigned slot=0;slot<0x800;slot+=64) {
      if(word(ram,0x17a2)&1)break;
      // AI bank is one byte; the adjacent byte stores hurt-AI time.
      unsigned a=0xf78+slot,id=word(ram,a),bank=ram[a+46],ptr=word(ram,a+22);
      // Gunship retains its independently qualified dedicated implementation.
      if(!id || id==0xd07f || id==0xd0bf || (word(ram,a+14)&0x300) ||
         bank<0xa0 || bank>0xb3 || ptr<0x8000 || word(ram,a+34)>7)continue;
      int ox=shipSigned(word(ram,a+2)-word(ram,0x911)+word(ram,0x7010+slot),16);
      int oy=shipSigned(word(ram,a+6)-word(ram,0x915)+word(ram,0x7012+slot),16);
      // Do not resurrect enemies culled vertically, or remote room inhabitants.
      if(oy+8<0 || oy>248 || ox< -512 || ox>768)continue;
      unsigned pal=word(ram,a+30),freeze=word(ram,a+38);
      if(word(ram,a+36) && (word(ram,0xe44)&2))pal=0;
      else if(freeze && (freeze>=90 || (freeze&2)))pal=0xc00;
      std::vector<Piece> candidate;
      bool valid=true;
      auto decode=[&](unsigned pointer,int x,int y) {
        if(pointer<0x8000){valid=false;return;}
        unsigned pos=romAddress(bank,pointer),count=word(rom,pos);
        // FFFE entries are background tilemaps, not sprites.
        if(count>128 || pointer+2+count*5>0x10000){valid=false;return;}
        for(unsigned n=0;n<count;n++) {
          unsigned p=pos+2+n*5,raw=word(rom,p);
          candidate.push_back({x+shipSigned(raw,9),y+shipSigned(rom[p+2],8),
            raw&0x8000?16u:8u,((word(rom,p+3)+word(ram,a+32))&65535)|pal,
            smpEnemyOrder(word(ram,a+34),slot,unsigned(candidate.size()))});
        }
      };
      if(word(ram,a+16)&4) {
        unsigned pos=romAddress(bank,ptr),count=word(rom,pos)&255;
        if(count>32 || ptr+2+count*8>0x10000)continue;
        for(unsigned n=0;n<count;n++) {
          unsigned p=pos+2+n*8,sub=word(rom,p+4);
          if(sub<0x8000 || word(rom,romAddress(bank,sub))==0xfffe)continue;
          decode(sub,ox+shipSigned(word(rom,p),16),oy+shipSigned(word(rom,p+2),16));
        }
      } else decode(ptr,ox,oy);
      if(!valid)continue;
      // A9:C710 parks the baby at x320 while C7CC counts down to its swoop.
      // That staging location is now inside the expanded arena. Keep only
      // this waiting pose hidden, including any native9-bit OAM pieces.
      if(motherArena && id==0xecbf && bank==0xa9 && word(ram,a+48)==0xc7cc) {
        for(const auto& q:candidate)for(unsigned i=0;i<128;i++) {
          unsigned high=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
          int delta=shipSigned((ram[0x370+i*4]|((high&1)<<8))-unsigned(q.x),9);
          if(std::abs(delta)<=1 && (unsigned(q.y)&255)==ram[0x371+i*4] &&
             (attr&~0xe00u)==(q.attr&~0xe00u) && q.size==(high&2?16u:8u))hiddenNativeObjects[i]=true;
        }
        continue;
      }
      // Anchor on native OAM whenever any piece was drawn. This recovers the
      // draw-time shake offset/palette and native order without advancing AI.
      // A matching palette needs no override: Torizo parts use distinct
      // palettes in their own map entries. Only correct a witnessed mismatch.
      bool witnessed=false,paletteOverride=false;int shift=0;unsigned nativePal=pal;
      for(const auto& q:candidate) {
        for(unsigned i=0;i<128;i++) {
          unsigned high=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
          unsigned px=ram[0x370+i*4]|((high&1)<<8);
          int delta=shipSigned(px-unsigned(q.x),9);
          if(delta< -1 || delta>1 || (unsigned(q.y)&255)!=ram[0x371+i*4] ||
             (attr&~0xe00u)!=(q.attr&~0xe00u) || q.size!=(high&2?16u:8u))continue;
          witnessed=true;shift=delta;nativePal=attr&0xe00;
          paletteOverride=nativePal!=(q.attr&0xe00);break;
        }
        if(witnessed)break;
      }
      // Gold ninjas can lose all native OAM as their origin crosses x=0/256,
      // while their extended spritemap still overlaps both the center and margin.
      // Keep that current pose; the caller fills center pixels only when its
      // background reference matches and no native object occupies the pixel.
      const bool ninjaEdge=id==0xf593 && bank==0xb2 && word(ram,0x79b)==0xb62b &&
        word(ram,0x7bb)==0xb638 && (ox<0 || ox>=256);
      if(!witnessed && !ninjaEdge && !(staging && id==0xddbf) && ox+int(word(ram,a+10))>=0 && ox-int(word(ram,a+10))<=256)continue;
      const bool kihunter=id>=0xeabf && id<=0xebff && ((id-0xeabf)%64)==0;
      for(auto q:candidate) {
        q.stageShift=staging && id==0xddbf;
        q.centerFill=!witnessed && (ninjaEdge || (kihunter && (ox+int(word(ram,a+10))<0 || ox-int(word(ram,a+10))>256)));
        q.x+=shift;if(witnessed && paletteOverride && !(staging && id==0xddbf))q.attr=(q.attr&~0xe00u)|nativePal;
        int visibleX=q.x-(q.stageShift?43:0);
        // Keep the whole staged actor for native-OAM identification, even
        // pieces translated completely offscreen. Otherwise their old OAM
        // copies can be mistaken for unrelated objects and left behind.
        if(!q.stageShift && (visibleX+int(q.size)<=-43 || visibleX>=rightEdge || q.y+int(q.size)<=0 || q.y>=224))continue;
        // Retail drawing layers run in ascending order. Slot order is the
        // experimental tie-breaker where native offscreen order is absent.
        pieces.push_back(q);
      }
    }
    // Mother Brain bypasses ordinary enemy drawing (her invisible flag disables
    // that path). A9:9357/9303 draw the committed custom brain/neck maps through
    // the active graphics hook. Sample them without executing their scripts.
    unsigned hook=word(ram,0x178c);
    if(word(ram,0x79b)==0xdd58 && word(ram,0x7bb)==0xdd6e &&
       word(ram,0xf78)==0xec7f && word(ram,0xfb8)==0xec3f &&
       ram[0xfa6]==0xa9 && ram[0xfe6]==0xa9 && ram[0x178e]==0xa9 &&
       (hook==0x87dd || hook==0x87c9)) {
      unsigned pc=word(ram,0x8002);
      auto custom=[&](unsigned pointer,int worldX,int worldY,unsigned pal,unsigned order) {
        if(pointer<0x8000)return;
        unsigned pos=romAddress(0xa9,pointer),count=word(rom,pos);
        if(count>128 || pointer+2+count*5>0x10000)return;
        std::vector<Piece> actor;
        for(unsigned n=0;n<count;n++) {
          unsigned p=pos+2+n*5,raw=word(rom,p);
          actor.push_back({worldX-int(word(ram,0x911))+shipSigned(raw,9),
            worldY-int(word(ram,0x915))+shipSigned(rom[p+2],8),
            raw&0x8000?16u:8u,word(rom,p+3)|pal,order+n});
        }
        // Native OAM supplies the exact draw-time shake and flashing palette
        // whenever a piece survives clipping. Offscreen-only pieces keep the
        // same native descriptor, rather than wrapping OAM's nine-bit X.
        int dx=0,dy=0;bool anchored=false,paletteOverride=false;unsigned nativePal=pal;
        for(const auto& q:actor) {
          for(unsigned i=0;i<128;i++) {
            unsigned high=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
            unsigned rawX=ram[0x370+i*4]|((high&1)<<8);
            int sx=shipSigned(rawX-unsigned(q.x),9),sy=shipSigned(unsigned(ram[0x371+i*4])-unsigned(q.y),8);
            if(std::abs(sx)>4 || std::abs(sy)>4 || (attr&~0xe00u)!=(q.attr&~0xe00u) || q.size!=(high&2?16u:8u))continue;
            dx=sx;dy=sy;nativePal=attr&0xe00;paletteOverride=nativePal!=(q.attr&0xe00);anchored=true;break;
          }
          if(anchored)break;
        }
        for(auto q:actor) {
          q.x+=dx;q.y+=dy;if(anchored && paletteOverride)q.attr=(q.attr&~0xe00u)|nativePal;
          if(q.x+int(q.size)<=-43 || q.x>=rightEdge || q.y+int(q.size)<=0 || q.y>=224)continue;
          pieces.push_back(q);
        }
      };
      if(pc>=0x8000 && pc<=0xfffc && word(rom,romAddress(0xa9,pc))<0x8000) {
        unsigned flash=word(ram,0xfdc),shake=word(ram,0x7840);
        if(!shake)shake=flash?flash:word(ram,0xfe0);
        unsigned at=(shake&6)/2;
        int sx=shipSigned(word(rom,romAddress(0xa9,0x93bb)+at*2),16);
        int sy=shipSigned(word(rom,romAddress(0xa9,0x93c3)+at*2),16);
        custom(word(rom,romAddress(0xa9,pc)+2),int(word(ram,0xfba))+sx,int(word(ram,0xfbe))+sy,
          flash&1?0:word(ram,0x781a),smpEnemyOrder(word(ram,0xfda),64,0));
      }
      if(hook==0x87c9 && !(word(ram,0xf86)&0x100)) {
        unsigned at=(word(ram,0xfdc)&6)/2;
        int sx=shipSigned(word(rom,romAddress(0xa9,0x93bb)+at*2),16);
        int sy=shipSigned(word(rom,romAddress(0xa9,0x93c3)+at*2),16);
        for(unsigned v:{0x2eu,0x2bu,0x28u,0x25u,0x22u})
          custom(0xa694,int(word(ram,0x8040+(v-0x20)*2))+sx,int(word(ram,0x8040+(v-0x20)*2+2))+sy,
            word(ram,0x7818),smpEnemyOrder(word(ram,0xfda),64,128+(0x2e - v)*16));
      }
    }
    // Samus's five beam/missile and five bomb slots use bank93 maps with complete OBJ
    // attributes. Their native lifetimes/collision remain entirely untouched.
    if(!(word(ram,0x93f)&0x8000))for(int slot=18;slot>=0;slot-=2) {
      if(!word(ram,0xc40+slot))continue;
      unsigned type=word(ram,0xc18+slot),pointer=word(ram,0xcb8+slot);
      const bool explosion=(type&0xf00)>=0x300;
      if(slot>=10 && !explosion)continue;
      // Retail 93:837A hides the power-bomb seed after its fuse expires.
      if((type&0xf00)==0x300 && (slot<10 || !word(ram,0xc7c+slot)))continue;
      if(pointer<0x8000)continue;
      if(!(type&0xf10)) {
        unsigned mask=(type&12)?2:1;
        bool phase=bool(word(ram,0x5b6)&mask);
        bool draw=(type&12)?(bool(slot&2)==phase):(bool(slot&2)!=phase);
        if(!draw)continue;
      }
      int x=shipSigned(word(ram,0xb64+slot)-word(ram,0x911),16);
      int y=shipSigned(word(ram,0xb78+slot)-word(ram,0x915),16);
      if(y<0 || y>=256 || x< -64 || x>=(wideArena?400:320))continue;
      unsigned pos=romAddress(0x93,pointer),count=word(rom,pos);
      if(count>128 || pointer+2+count*5>0x10000)continue;
      for(unsigned n=0;n<count;n++) {
        unsigned p=pos+2+n*5,raw=word(rom,p);
        pieces.push_back({x+shipSigned(raw,9),y+shipSigned(rom[p+2],8),
          raw&0x8000?16u:8u,word(rom,p+3),smpObjectGroup(explosion?0:5)+unsigned(18-slot)*128+n});
      }
    }
    // The moved Crocomire blocker lets Samus enter the added margin while
    // the camera stays locked. Her native OAM is already drawn there: retain
    // its pose, flashing, palette, priority and order, using the reserved
    // Samus tiles00..1F/palette4 rather than inventing animation descriptors.
    unsigned crocoStage=word(ram,0xfa8);
    if(wideArena || (word(ram,0x79b)==0xa98d && crocoStage>=0x3e && crocoStage<=0x52 && !(crocoStage&1))) {
      int sx=shipSigned(word(ram,0xb04),16),sy=shipSigned(word(ram,0xb06),16);
      if(sx<16 || sx>=240)for(unsigned i=0;i<128;i++) {
        unsigned hi=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
        if((attr&0xe00)!=0x800 || (attr&0x1ff)>=0x20)continue;
        unsigned raw=ram[0x370+i*4]|((hi&1)<<8),size=hi&2?16:8;
        int x=sx+shipSigned(raw-unsigned(sx),9),y=ram[0x371+i*4];
        if(x<sx-32 || x>sx+32 || y<sy-48 || y>sy+48 || y>=224 ||
           x+int(size)<=-43 || x>=rightEdge || (x>=0 && x+int(size)<=256))continue;
        pieces.push_back({x,y,size,attr,smpObjectGroup(5)+i});
      }
    }
    // Pickups and Mini Kraid spit/spikes use bank8D enemy-projectile maps.
    // Read current descriptors only; never spawn or advance these objects.
    int shakeX=0,shakeY=0;
    unsigned quake=word(ram,0x183e);
    if(word(ram,0x1840) && !word(ram,0xa78) && quake<36) {
      unsigned p=romAddress(0x86,0x846b)+quake*4;
      int sign=(word(ram,0x1840)&2)?-1:1;
      shakeX=shipSigned(word(rom,p),16)*sign;shakeY=shipSigned(word(rom,p+2),16)*sign;
    }
    for(int slot=34;slot>=0;slot-=2) {
      unsigned id=word(ram,0x1997+slot);
      bool miniKraid=id==0x9db0 || id==0x9dbe || id==0x9dcc;
      bool pirateLaser=id==0xa17b;
      bool deathExplosion=id==0xf345;
      bool cacatacSpike=id==0xdafe;
      bool downwardGate=id==0xe659;
      // Draygon wall turrets animate their charge and shot in the same live
      // projectile slot (86:8CA4). Extend that current map into the margins.
      // Goop8E50 is a separate live projectile, including its birth frames.
      // Sampling both descriptors leaves native spawning and lifetimes intact.
      bool draygonProjectile=word(ram,0x79b)==0xda60 && (id==0x8e5e || id==0x8e50);
      unsigned effectInstruction=word(ram,0x1b47+slot);
      bool endingEffect=staging && (id==0x90c1 || (id==0xe509 && effectInstruction>=0xe2f2 && effectInstruction<=0xe302));
      // CB59 bombs spawn9650, then966C/967A horizontal afterburn pieces.
      // Sample their existing maps beyond the native edge; spawning, damage
      // and collision remain under the original projectile routines.
      bool motherProjectile=motherArena && ((id>=0xcb13 && id<=0xcbad && (id-0xcb13)%14==0) || id==0xc17e || id==0xc18c || id==0xe509 || id==0x9650 || id==0x966c || id==0x967a);
      if(!id || (word(ram,0x1a03+slot)!=0xefe0 && !miniKraid && !pirateLaser && !deathExplosion && !cacatacSpike && !downwardGate && !draygonProjectile && !endingEffect && !motherProjectile))continue;
      unsigned pointer=word(ram,0x1b6b+slot),gfx=word(ram,0x19bb+slot);
      if(pointer<0x8000)continue;
      int x=shipSigned(word(ram,0x1a4b+slot)-word(ram,0x911),16)+shakeX;
      int y=shipSigned(word(ram,0x1a93+slot)-word(ram,0x915),16)+shakeY;
      if(x< -128 || x>=384 || y< -128 || y>=384)continue;
      unsigned pos=romAddress(0x8d,pointer),count=word(rom,pos);
      if(count>128 || pointer+2+count*5>0x10000)continue;
      for(unsigned n=0;n<count;n++) {
        unsigned p=pos+2+n*5,raw=word(rom,p);
        pieces.push_back({x+shipSigned(raw,9),y+shipSigned(rom[p+2],8),
          raw&0x8000?16u:8u,((word(rom,p+3)+(gfx&255))&65535)|(gfx&0xff00),
          smpObjectGroup(word(ram,0x1bd7+slot)&0x1000?1:9)+unsigned(34-slot)*128+n});
        pieces.back().stageShift=endingEffect;
      }
    }
    // Retail B4:BD32 sprite objects: impact/dud/death/dust effects are a
    // separate pool from Samus projectiles and enemy projectiles. Read their
    // current instruction's spritemap; do not advance timers or spawn effects.
    for(int slot=62;slot>=0;slot-=2) {
      unsigned instruction=word(ram,0xef78+slot);
      if(instruction<0x8000 || instruction>0xfffc)continue;
      unsigned pointer=word(rom,romAddress(0xb4,instruction)+2);
      if(pointer<0x8000)continue;
      int x=shipSigned(word(ram,0xf0f8+slot)-word(ram,0x911),16);
      int y=shipSigned(word(ram,0xf1f8+slot)-word(ram,0x915),16);
      if(x< -128 || x>=384 || y<0 || y>=272)continue;
      unsigned pos=romAddress(0xb4,pointer),count=word(rom,pos),gfx=word(ram,0xf078+slot);
      if(count>128 || pointer+2+count*5>0x10000)continue;
      for(unsigned n=0;n<count;n++) {
        unsigned p=pos+2+n*5,raw=word(rom,p);
        pieces.push_back({x+shipSigned(raw,9),y+shipSigned(rom[p+2],8),
          raw&0x8000?16u:8u,((word(rom,p+3)+(gfx&0x1ff))&65535)|(gfx&0xe00),
          unsigned(62-slot)*128+n});
      }
    }
    if(staging) {
      // Recompose only witnessed center pixels. Preserve native OAM ordering
      // for Samus and unrelated effects; translate the identified actor/debris.
      auto decoded=pieces;pieces.clear();
      std::vector<bool> found(decoded.size());
      for(unsigned i=0;i<128;i++) {
        unsigned hi=ram[0x570+i/4]>>(i%4*2),attr=word(ram,0x372+i*4);
        unsigned raw=ram[0x370+i*4]|((hi&1)<<8),size=hi&2?16:8;
        int x=shipSigned(raw,9),y=ram[0x371+i*4];
        if(y>=224)continue;
        Piece q{x,y,size,attr,i,i,true,false};
        for(size_t j=0;j<decoded.size();j++) {
          auto& d=decoded[j];
          bool position=(unsigned(d.x)&511)==raw && (unsigned(d.y)&255)==unsigned(y);
          // Enemy large-piece Y adjustment and projectile quake sampling
          // differ from the descriptor origin. Native OAM is authoritative.
          if(d.stageShift && std::abs(shipSigned(unsigned(d.x)-raw,9))<=2 &&
             std::abs(shipSigned(unsigned(d.y)-unsigned(y),8))<=3)position=true;
          if(position && d.size==size && d.attr==attr) {
            found[j]=true;q.x=d.x+shipSigned(raw-unsigned(d.x),9);q.stageShift|=d.stageShift;
          }
        }
        referencePieces.push_back(q);
        if(q.stageShift)q.x-=43;
        pieces.push_back(q);
      }
      // Native OAM can omit fully offscreen parts. Their decoded maps extend
      // the same translated actor; no simulation or animation is advanced.
      for(size_t j=0;j<decoded.size();j++)if(!found[j]) {
        auto q=decoded[j];if(q.stageShift)q.x-=43;
        q.centerFill=q.stageShift;q.sortOrder=128+q.order;
        pieces.push_back(q);
      }
    }
    // Within each native draw group, preserve witnessed OAM coverage before
    // adding offscreen-only pieces. Otherwise a culled wing can cover another
    // enemy in the margin while remaining absent from the native center.
    std::array<int,512> firstAtX;firstAtX.fill(-1);std::array<int,128> nextAtX;
    for(int i=127;i>=0;i--) {
      unsigned high=ram[0x570+i/4]>>(i%4*2),x=ram[0x370+i*4]|((high&1)<<8);
      nextAtX[i]=firstAtX[x];firstAtX[x]=i;
    }
    if(!staging)for(auto& q:pieces) {
      unsigned group=q.order&0xfff00000u;
      q.sortOrder=group+0x80000+(q.order&0xfffff);
      for(int i=firstAtX[unsigned(q.x)&511];i>=0;i=nextAtX[i]) {
        unsigned high=ram[0x570+i/4]>>(i%4*2);
        if((unsigned(q.x)&511)==(ram[0x370+i*4]|((high&1)<<8)) &&
           (unsigned(q.y)&255)==ram[0x371+i*4] && q.attr==word(ram,0x372+i*4) && q.size==(high&2?16u:8u)) {
          q.sortOrder=group+i*256;break;
        }
      }
    }
    std::stable_sort(pieces.begin(),pieces.end(),[](auto a,auto b){return a.sortOrder<b.sortOrder;});
    if(staging) {
      auto mark=[&](const Piece& q){if(q.stageShift)for(int y=std::max(0,q.y);y<std::min(224,q.y+int(q.size));y++) {
        stagingBounds[y].first=std::min(stagingBounds[y].first,q.x);
        stagingBounds[y].second=std::max(stagingBounds[y].second,q.x+int(q.size));
      }};
      for(const auto& q:pieces)mark(q);
      for(const auto& q:referencePieces)mark(q);
    }
    indexRows();
  }
  size_t count() const{return pieces.size();}
  bool hasCenterFill() const {return centerFill;}
  bool stagingCandidate(int x,unsigned line) const {return staging && line>=1 && line<=224 && x>=stagingBounds[line-1].first && x<stagingBounds[line-1].second;}
  // Conservatively retain every opaque native object pixel, regardless of
  // layer. Native sprite overlap is never replaced by this edge repair.
  bool ceresTimerAt(int x,unsigned line,int shift=0) const {
    if(line<1 || line>224)return false;
    auto range=ceresTimerBounds[line-1];return x-shift>=range.first && x-shift<range.second;
  }
  std::optional<ShipPixel> nativePixel(int x,unsigned line,int timerShift=0) const {
    bool indexed=arenaNative && line>=32 && line<=224;
    unsigned count=indexed?arenaNativeCounts[line-1]:128;
    for(unsigned n=0;n<count;n++) {
      unsigned i=indexed?arenaNativeRows[line-1][n]:n;
      if(hiddenNativeObjects[i])continue;
      unsigned hi=nativeOam[512+i/4]>>(i%4*2),size=hi&2?16:8;
      unsigned px=nativeOam[i*4]|((hi&1)<<8),py=nativeOam[i*4+1],attr=word(nativeOam,i*4+2);
      unsigned dx=(unsigned(x-(ceresTimerObjects[i]?timerShift:0))-px)&511,dy=(line-1-py)&255;
      if(dx>=size || dy>=size)continue;
      if(attr&0x4000)dx=size-1-dx;if(attr&0x8000)dy=size-1-dy;
      unsigned base=registers.get(Reg::obj_tiledataAddress);if(attr&256)base+=(1+registers.get(Reg::obj_nameselect))<<12;
      unsigned tile=(((attr&255)/16+dy/8)&15)*16+((attr+dx/8)&15),address=((base+tile*16)&32767)*2,index=0;
      for(unsigned b=0;b<4;b++)index|=((vram[(address+b/2*16+dy%8*2+b%2)&65535]>>(7-dx%8))&1)<<b;
      if(index) {
        unsigned pal=(attr>>9)&7;
        return ShipPixel{unsigned(word(palette,(128+pal*16+index)*2)&32767),
          registers.get(Reg(unsigned(Reg::obj_priority0)+((attr>>12)&3))),pal,i};
      }
    }
    return {};
  }
  bool nativeObjectAt(int x,unsigned line) const {return bool(nativePixel(x,line));}
  bool recomposeArenaWall(int x,unsigned line) const {return arenaNative && x>=arenaNativeLeft && x<256 && line>=32 && line<=224;}
  std::optional<ShipPixel> pixel(int x,unsigned line,bool fillNative=false) const {
    bool center=x>=0 && x<256;
    // The expanded arenas contain custom native sprites: Ridley's tail/baby,
    // Mother Brain's Metroid, and Samus. Their committed9-bit OAM already
    // contains those pieces. Preserve its exact order and priority.
    // At the old wall, use these same objects for both native-reference and
    // relocated-background composition, so occluded Samus pixels don't leave
    // a silhouette of the removed wall behind them.
    if(arenaNative && x>=arenaNativeLeft && x<342) {
      if(auto p=nativePixel(x,line))return p;
      if(center)return {};
    }
    if(center && !fillNative && !staging)return {};
    bool reference=staging && center && !fillNative;
    if(!reference && (x<minX || x>=maxX))return {};
    auto range=line>=1 && line<=224?rowRanges[line-1]:std::pair<size_t,size_t>{0,pieces.size()};
    if(reference)range={0,referencePieces.size()};
    for(size_t i=range.first;i<range.second;i++) {
      const auto& q=reference?referencePieces[i]:pieces[i];
      if(center && !q.centerFill)continue;
      int dx=x-q.x,dy=int(line)-1-q.y;
      if(dx<0 || dy<0 || dx>=int(q.size) || dy>=int(q.size))continue;
      if(q.attr&0x4000)dx=int(q.size)-1-dx;
      if(q.attr&0x8000)dy=int(q.size)-1-dy;
      unsigned base=registers.get(Reg::obj_tiledataAddress);
      if(q.attr&256)base+=(1+registers.get(Reg::obj_nameselect))<<12;
      unsigned tile=(((q.attr&255)/16+unsigned(dy)/8)&15)*16+((q.attr+unsigned(dx)/8)&15);
      unsigned address=((base+tile*16)&32767)*2,index=0;
      for(unsigned bit=0;bit<4;bit++)index|=((vram[(address+(bit/2)*16+(dy%8)*2+bit%2)&65535]>>(7-dx%8))&1)<<bit;
      if(!index)continue;
      unsigned pal=(q.attr>>9)&7;
      return ShipPixel{unsigned(word(palette,(128+pal*16+index)*2)&32767),
        registers.get(Reg(unsigned(Reg::obj_priority0)+((q.attr>>12)&3))),pal,q.order};
    }
    return {};
  }
};
}
