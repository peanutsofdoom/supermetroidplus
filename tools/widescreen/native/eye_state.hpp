// SPDX-License-Identifier: GPL-3.0-or-later
// Effect ownership follows committed WRAM, independently of terrain uploads.
#pragma once
#include "registers.hpp"
#include "xray.hpp"
#include <array>
#include <optional>
#include <vector>
#include <algorithm>
namespace SMPWide {
struct EyeBeam {
  int x,y;
  unsigned lowSlope,highSlope;
  static int floorPixel(int value){return value>=0?value/256:-((-value+255)/256);}
  std::optional<std::pair<int,int>> edges(int line) const {
    int dy=line-y;if(dy<0)return {};
    return {{floorPixel(x*256+int(lowSlope)*dy),floorPixel(x*256+int(highSlope)*dy)}};
  }
  std::pair<int,int> nativeBounds(int line) const {
    auto e=edges(line);if(!e || e->second<0)return {255,0};
    if(e->first>255){int before=x*256+int(lowSlope)*(line-y-1);return before>=0 && before<65536?std::pair{255,255}:std::pair{255,0};}
    return {std::max(0,e->first),std::min(255,e->second)};
  }
};
struct EyeRow {
  std::optional<EyeBeam> beam;
  int line,left,right;
  bool suit=false;
  int extendedLeft=0,extendedRight=-1;
  bool powerBomb=false;
  bool xray=false;
  bool motherBrain=false;
  bool trackingEye=false;
  bool inside(int x) const {
    if(x>=0 && x<256)return left<=x && x<=right;
    if(suit || powerBomb || xray || motherBrain || trackingEye)return extendedLeft<=x && x<=extendedRight;
    auto e=beam?beam->edges(line):std::nullopt;
    return e && e->first<=x && x<=e->second;
  }
};
class EyeState {
  std::array<uint16_t,128> tangents{};
  bool configured=false;
  struct MotherEdge {int native=0,edge=0;bool known=false;};
  std::array<MotherEdge,512> motherEdges{};
  MotherEdge motherLeft{},motherRight{};
  void captureMotherBrain(unsigned address,unsigned value,unsigned width) {
    if(address<0x9d00 || address+width>0x9f00 || !configured ||
       word(ram,0x79b)!=0xdd58 || word(ram,0x7bb)!=0xdd6e)return;
    const int index=(address-0x9d00)/2,origin=word(ram,0x803a);
    const unsigned a=word(ram,0x8034),b=word(ram,0x8036);
    int lo=255,hi=0,native=255;
    // Reconstruct the unbounded 8.8 accumulators when retail commits each
    // table word. HDMA fetch selects that exact version below, even on lag.
    if(a<64 && b<64) {
      int dy=index-(origin-32)-1;
      if(index>=2 && dy>0) {
        lo=(int(word(ram,0x8038))+int(tangents[a])*dy)>>8;
        hi=(int(word(ram,0x803c))+int(tangents[b])*dy)>>8;
        native=std::min(lo,255)|(std::min(hi,255)<<8);
        if(native==65535)native=255;
      }
    }else if(a<64 && b>=64 && b<128 && index>=2) {
      int split=origin-31;
      int dy=index<=split?split-index+1:index-split;
      lo=(int(word(ram,0x8038))+int(tangents[index<=split?b:a])*dy)>>8;
      hi=32767;native=lo<256?(lo|0xff00):255;
    }else return;
    for(unsigned i=0;i<width;i++) {
      unsigned byte=address+i,shift=(byte&1)*8,v=(value>>(i*8))&255;
      motherEdges[byte-0x9d00]={int(v),(byte&1)?hi:lo,int(v)==((native>>shift)&255)};
    }
  }
  const char* motherBrainRow(const Registers& r,int line,std::optional<EyeRow>& out) const {
    int left=r.get(Reg::window_oneLeft),right=r.get(Reg::window_oneRight);
    if(!motherLeft.known || !motherRight.known || motherLeft.native!=left || motherRight.native!=right)
      return "Mother Brain beam fetch phase mismatch";
    out=EyeRow{{},line,left,right,false,motherLeft.edge,motherRight.edge,false,false,true};return nullptr;
  }
  std::vector<uint8_t> ram;
  std::optional<EyeBeam> beam;
  std::optional<XrayCone> trackingBeam;
  bool eyeRoom() const {
    if(ram.size()!=131072)return false;
    const unsigned room=word(ram,0x79b),state=word(ram,0x7bb);
    // The pre-awakening E6BF scanner also appears in Blue Brinstar's energy
    // tank room, facing left. Both use the same retail eye/X-ray HDMA builder;
    // retain the exact per-row native-window witness and all angle guards.
    return (room==0x9e9f && state==0x9eb1) || (room==0x9f64 && state==0x9f76);
  }
  std::array<uint8_t,128> suitCurve{};
  bool suitConfigured=false;
  mutable unsigned suitTailOwners=0,suitRoom=0;
  std::vector<uint8_t> pbRom;
  std::vector<uint8_t> xrayRom;
  mutable unsigned xrayTail=0,xrayRoom=0;
  const char* xrayRow(const Registers& r,int line,std::optional<EyeRow>& out) const {
    bool active=xrayOwned(ram);
    if(active){xrayTail=16;xrayRoom=word(ram,0x79b);}
    if(!active && (!xrayTail || ram.empty() || word(ram,0x79b)!=xrayRoom))return "unqualified X-ray window";
    int left=int(r.get(Reg::window_twoLeft)),right=int(r.get(Reg::window_twoRight));
    int lo=1,hi=0;
    if(active && word(ram,0xa7a)<3 && !xrayRom.empty()) {
      XrayCone cone{int(int16_t(word(ram,0xaf6)-word(ram,0x911)))+((ram[0xa1e]==4)?-3:3),
        int(int16_t(word(ram,0xafa)-word(ram,0x915)))-(ram[0xa1f]==5?12:16),word(ram,0xa82),word(ram,0xa84)};
      if(cone.angle>256 || cone.width>10)return "unqualified X-ray angle";
      auto edges=cone.edges(line-1,xrayRom);lo=edges.first;hi=edges.second;
      // Retail keeps a one-pixel endpoint on the first fully clipped row.
      // Join that exact native endpoint to the extended cone without a gap.
      if(hi>=lo && left==255 && right==255 && lo>255)lo=255;
      if(hi>=lo && left==0 && right==0 && hi<0)hi=0;
    }
    out=EyeRow{{},line,left,right,false,lo,hi,false,true};return nullptr;
  }
  std::array<int,192> pbWidths{};
  struct PBEdge {int native=0,edge=0;bool known=false,saturated=false;};
  std::array<PBEdge,192> pbLeft{},pbRight{};PBEdge pbFetchLeft{},pbFetchRight{};
  bool pbValid=false;unsigned pbRoom=0,pbIndex=0,pbTail=0;
  int pbX=0;
  unsigned pbStage=0,pbRadius=0,pbPointer=0;
  void capturePowerBomb() {
    if(pbRom.empty() || !(word(ram,0x592)&0x8000))return;
    unsigned stage=0;
    for(unsigned slot=0;slot<12;slot+=2)if(word(ram,0x18b4+slot) && (word(ram,0x18fc+slot)&255)==0x88){
      auto p=word(ram,0x18f0+slot);if(p==0x90df || p==0x91a8 || p==0x8de9 || p==0x8eb2)stage=p;
    }
    if(!stage){pbValid=false;return;}
    unsigned radius=word(ram,stage==0x90df?0xcec:0xcea)>>8,pointer=word(ram,0xcf2),index=word(ram,0xce8);
    int x=int(word(ram,0xce6))-256;
    if(pbValid && pbStage==stage && pbRadius==radius && pbPointer==pointer && pbIndex==index && pbX==x)return;
    pbStage=stage;pbRadius=radius;pbPointer=pointer;pbIndex=index;pbX=x;pbRoom=word(ram,0x79b);pbTail=2;pbValid=true;
    pbWidths.fill(-1);
    if(stage==0x90df || stage==0x8de9){
      int row=(radius*pbRom[0x2286])>>8,last=0;
      for(unsigned i=0;i<32;i++){
        int target=(radius*pbRom[0x2286+i])>>8;last=(radius*pbRom[0x2266+i])>>8;
        for(int j=row;j>=target;j--)if(j<192)pbWidths[j]=last;
        row=target;
      }
      for(int j=row;j>=0;j--)pbWidths[j]=last;
    }else if(pointer>=0x9246 && pointer<=0xa146){
      for(unsigned i=0;i<192;i++)pbWidths[i]=pbRom[(pointer&32767)+i]?pbRom[(pointer&32767)+i]:-1;
    }else pbValid=false;
  }
  const char* powerBombRow(const Registers& r,int line,std::optional<EyeRow>& out) const {
    if(!pbValid || pbRoom!=word(ram,0x79b) || (!(word(ram,0x592)&0x8000) && !pbTail))return "unqualified Power Bomb window";
    int left=int(r.get(Reg::window_twoLeft)),right=int(r.get(Reg::window_twoRight));
    int lo=255,hi=0;bool saturated=false;
    if(pbFetchLeft.known && pbFetchRight.known) {
      if(left!=pbFetchLeft.native || right!=pbFetchRight.native)return "Power Bomb fetch phase mismatch";
      lo=pbFetchLeft.edge;hi=pbFetchRight.edge;saturated=pbFetchLeft.saturated && pbFetchRight.saturated;
    }else {
      unsigned at=0x9800+3*(pbIndex+unsigned(line-1));
      if(at+2>=pbRom.size())return "Power Bomb HDMA index";
      unsigned address=word(pbRom,at+1);int width=-1;
      if(address>=0xc406 && address<0xc4c6)width=pbWidths[address-0xc406];
      if(width>=0){lo=pbX-width;hi=pbX+width;saturated=width==255;}
    }
    if(hi>=lo && hi>=0 && lo<=255) {
      if(std::clamp(lo,0,255)!=left || std::clamp(hi,0,255)!=right)return "Power Bomb native window mismatch";
      if(saturated && left==0 && right==255){lo=std::min(lo,-43);hi=std::max(hi,298);}
    }else if(left<=right)return "Power Bomb closed window mismatch";
    out=EyeRow{{},line,left,right,false,lo,hi,true};return nullptr;
  }
  mutable std::array<std::optional<EyeRow>,225> suitRows{};
public:
  bool ceresPowerBombTail() const {
    if(ram.size()!=131072 || (word(ram,0x592)&0x8000))return false;
    unsigned game=word(ram,0x998),function=word(ram,0x1f51);
    return (game==0x22 || game==0x25) && (function==0xc610 || function==0xc627 || function==0xc699);
  }
  bool messageOverlay() const {return !ram.empty() && ram[0x5b]==0x58 && word(ram,0x1c1f)!=0;}
  void powerBombFetch(unsigned address) {
    if(!ram.empty() && word(ram,0x79b)==0xdd58) {
      if(address>=0x7e9d00 && address<0x7e9f00) {
        auto e=motherEdges[address-0x7e9d00];
        if(address&1)motherRight=e;else motherLeft=e;
      }else if(address>=0x7e9c00 && address<0x7e9c06) {
        unsigned v=ram[address&0xffff];
        if(address==0x7e9c00 || address==0x7e9c03)motherLeft={int(v),int(v),true};
        else motherRight={int(v),int(v),true};
      }
    }
    if(address>=0x7ec406 && address<0x7ec4c6)pbFetchLeft=pbLeft[address-0x7ec406];
    else if(address>=0x7ec506 && address<0x7ec5c6)pbFetchRight=pbRight[address-0x7ec506];
    else if(address==0x7ec606)pbFetchLeft={int(ram.empty()?255:ram[0xc606]),255,true,false};
    else if(address==0x7ec607)pbFetchRight={int(ram.empty()?0:ram[0xc607]),0,true,false};
  }
  void configurePowerBomb(Bytes rom){pbRom.assign(rom.begin()+0x40000,rom.begin()+0x50000);}
  void configureXray(Bytes rom){xrayRom.assign(rom.begin(),rom.end());}
  void configureSuit(Bytes curve) {
    if(curve.size()!=128)throw std::invalid_argument("suit curve shape");
    std::copy(curve.begin(),curve.end(),suitCurve.begin());suitConfigured=true;
  }
  void configure(Bytes data) {
    if(data.size()!=256 || !ram.empty())throw std::invalid_argument("eye table setup");
    for(unsigned i=0;i<128;i++)tangents[i]=word(data,i*2);
    configured=true;
  }
  void reset(){ram.clear();beam.reset();trackingBeam.reset();suitTailOwners=0;suitRoom=0;suitRows={};pbValid=false;pbTail=0;pbLeft={};pbRight={};pbFetchLeft={};pbFetchRight={};xrayTail=0;motherEdges={};motherLeft={};motherRight={};}
  void owner(Bytes data) {
    if(data.size()!=131072)throw std::invalid_argument("eye owner size");
    if(ram.empty())ram.assign(data.begin(),data.end());
    else if(!std::equal(ram.begin(),ram.end(),data.begin()))throw std::invalid_argument("eye committed mirror mismatch");
    if(suitTailOwners)--suitTailOwners;
    if(xrayTail)--xrayTail;
    if(!word(data,0x592) && pbTail)--pbTail;
  }
  void write(unsigned address,unsigned value,unsigned width) {
    if(ram.empty())return;
    if((width!=1 && width!=2) || address>ram.size()-width || value>=(1u<<(width*8)))throw std::invalid_argument("invalid eye write");
    captureMotherBrain(address,value,width);
    if(address>=0x9100 && address<0x9300 && !word(ram,0x592) && word(ram,0x9000) && eyeRoom()) {
      int angle=word(ram,0xfee),widthAngle=word(ram,0x192c);
      int x=int(word(ram,0xfba))-int(word(ram,0x911)),y=int(word(ram,0xfbe))-int(word(ram,0x915));
      beam.reset();trackingBeam.reset();
      // The eye follows Samus across quadrant boundaries, including124+4=128.
      // Its retail table builder is the same one used by X-ray. Latch inputs
      // on the actual table write; never throw for a normal tracking angle on
      // the emulator's coroutine stack. Unknown inputs fall back at compose.
      if(configured && 64<angle-widthAngle && angle-widthAngle<=angle+widthAngle && angle+widthAngle<128)
        beam=EyeBeam{x,y,tangents[angle+widthAngle],tangents[angle-widthAngle]};
      if(!xrayRom.empty() && angle<256 && widthAngle<=4)trackingBeam=XrayCone{x,y,unsigned(angle),unsigned(widthAngle)};
    }
    if((address>=0xc406 && address<0xc4c6) || (address>=0xc506 && address<0xc5c6)) {
      capturePowerBomb();
      for(unsigned i=0;i<width;i++) {
        unsigned a=address+i;bool left=a>=0xc406 && a<0xc4c6;unsigned row=left?a-0xc406:a-0xc506;
        if(row<192 && pbValid){int w=pbWidths[row];auto& edge=left?pbLeft[row]:pbRight[row];edge={int(uint8_t(value>>(i*8))),w>=0?pbX+(left?-w:w):(left?255:0),true,w==255};}
      }
    }
    for(unsigned i=0;i<width;i++)ram[address+i]=uint8_t(value>>(i*8));
  }
  const char* tryRow(const Registers& r,int line,std::optional<EyeRow>& output) const {
    output.reset();
    if(r.get(Reg::window_bg3_twoEnable) || r.get(Reg::window_col_twoEnable)) {
      if(xrayOwned(ram) || (xrayTail && !ram.empty() && !word(ram,0x592) && r.get(Reg::window_twoLeft)>r.get(Reg::window_twoRight)))return xrayRow(r,line,output);
      return powerBombRow(r,line,output);
    }
    if(!r.get(Reg::window_bg3_oneEnable))return nullptr;
    if(ram.size()==131072 && word(ram,0x79b)==0xdd58 && word(ram,0x7bb)==0xdd6e)
      return motherBrainRow(r,line,output);
    bool suitActive=false;
    if(ram.size()==131072) {
      suitActive=ram[0x1986]==0x12;
      for(unsigned slot=0;slot<12;slot+=2)if(word(ram,0x18b4+slot) && (word(ram,0x18fc+slot)&255)==0x88 &&
        (word(ram,0x18f0+slot)==0xe026 || word(ram,0x18f0+slot)==0xe05c))suitActive=true;
    }
    if(suitActive){suitTailOwners=2;suitRoom=word(ram,0x79b);}
    // The native window registers can outlive the suit's WRAM/HDMA teardown
    // by one upload owner. Only retain closed/interior-only window semantics
    // in the same room; never infer a new effect or extend a stale full flash.
    if(!suitActive && suitTailOwners && ram.size()==131072 && word(ram,0x79b)==suitRoom && word(ram,0x998)==8 && line>=32 && line<=224) {
      int left=r.get(Reg::window_oneLeft),right=r.get(Reg::window_oneRight);
      if(suitRows[line] && suitRows[line]->left==left && suitRows[line]->right==right){output=suitRows[line];return nullptr;}
      if(left>right || !(left==0 && right==255)){output=EyeRow{{},line,left,right,true,left,right};return nullptr;}
    }
    if(suitActive && line>=32 && line<=224) {
      unsigned stage=word(ram,0xdec);if(stage>6)return "unqualified suit stage";
      int left=r.get(Reg::window_oneLeft),right=r.get(Reg::window_oneRight),lo=left,hi=right;
      if(left<=right && (left==0 || right==255)) {
        if(stage==2 && suitConfigured) {
          unsigned row=unsigned(line-1),curve=suitCurve[std::min(row,255-row)];
          lo=int(ram[0xdee])-int(curve);hi=int(ram[0xdef])+int(curve);
          if(ram[0xdee]==0 && ram[0xdef]==255){lo=-43;hi=298;}
          if(std::clamp(lo,0,255)!=left || std::clamp(hi,0,255)!=right)return "suit window phase mismatch";
        } else if(stage>=3 && left==0 && right==255){lo=-43;hi=298;}
        else return "unqualified suit window edge";
      }
      output=EyeRow{{},line,left,right,true,lo,hi};suitRows[line]=output;return nullptr;
    }
    if(!eyeRoom() || line<32 || line>224)return "unqualified eye row";
    int left=r.get(Reg::window_oneLeft),right=r.get(Reg::window_oneRight);
    auto selected=beam;
    if(word(ram,0x9000)) {
      if(!selected || selected->nativeBounds(line)!=std::pair{left,right}) {
        if(!trackingBeam)return "unqualified eye angle";
        // Retail91:BEC2/BF72 starts the off-screen origin's upward sweep by
        // advancing its slope before the first write. On-screen91:C5FF writes
        // the origin row first. Keep that one-row distinction above the eye;
        // the downward sweep and ordinary Samus X-ray convention are unchanged.
        int row=line-1;
        bool horizontalLine=!trackingBeam->width && (trackingBeam->angle==64 || trackingBeam->angle==192);
        if(!horizontalLine && (trackingBeam->x<0 || trackingBeam->x>=256) && line<=trackingBeam->y)--row;
        auto [lo,hi]=trackingBeam->edges(row,xrayRom);
        bool match=hi>=lo && lo<=255 && hi>=0 && left==std::max(0,lo) && right==std::min(255,hi);
        // Retail retains an endpoint for one row before its empty sentinel.
        if(hi<lo || lo>255 || hi<0)match=left>right || (lo>255 && left==255 && right==255) || (hi<0 && left==0 && right==0);
        if(!match)return "eye window phase mismatch";
        output=EyeRow{{},line,left,right,false,lo,hi,false,false,false,true};return nullptr;
      }
    }else {
      selected.reset();bool dark=!(r.get(Reg::screen_colorRed)||r.get(Reg::screen_colorGreen)||r.get(Reg::screen_colorBlue));
      if(word(ram,0x9100+2*(line-1))!=0xff || !((left==255 && right==0)||(left==0 && right==0 && dark)))return "unqualified eye shutdown";
    }
    output=EyeRow{selected,line,left,right};return nullptr;
  }
  std::optional<EyeRow> row(const Registers& r,int line) const {
    std::optional<EyeRow> result;
    if(auto error=tryRow(r,line,result))throw std::invalid_argument(error);
    return result;
  }
};
}
