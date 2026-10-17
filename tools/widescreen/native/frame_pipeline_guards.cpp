// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_pipeline.hpp"
#include <iostream>
#include <functional>
using namespace SMPWide;
static void rejects(const char* reason,const std::function<void()>& action) {
  try {action();}catch(const std::exception& e) {
    if(std::string(e.what()).find(reason)!=std::string::npos)return;
    throw;
  }
  throw std::runtime_error("negative fixture was accepted");
}
int main() {
  try {
    std::vector<uint8_t> ram(131072),reg(205),zeroVram(65536),zeroPalette(512),native(512*480*3);
    ram[0x998]=8;ram[0x7a5]=16;ram[0x7a7]=16;
    reg[registerFields[unsigned(Reg::displayDisable)].offset]=1;
    auto v=memoryImage(zeroVram,65536),c=memoryImage(zeroPalette,512),r=memoryImage(reg,205);
    FramePipeline p;
    rejects("not writable",[&]{p.owner(1,ram);});
    p.begin();rejects("unfinished prior",[&]{p.begin();});p.owner(1,ram);
    rejects("upload owner mismatch",[&]{p.upload(2);});
    rejects("raster owner mismatch",[&]{p.raster(0,1,v,c,r);});
    p.upload(1);rejects("raster owner mismatch",[&]{p.raster(0,2,v,c,r);});
    p.raster(0,1,v,c,r);rejects("duplicate raster",[&]{p.raster(0,1,v,c,r);});
    rejects("not sealed",[&]{p.compose(native,false);});
    rejects("incomplete raster",[&]{p.seal();});
    for(unsigned line=1;line<=224;line++)p.raster(line,1,v,c,r);
    p.memoryWrite({1,1,0,1,0});rejects("transaction mismatch",[&]{p.registerWrite(2,250,0,0x2118,r);});
    rejects("unpaired frame",[&]{p.seal();});
    p.registerWrite(1,250,0,0x2118,r);p.seal();
    auto first=p.compose(native,false),again=p.compose(native,false);
    if(first.rgb!=again.rgb || first.rgb!=std::vector<uint8_t>(342*224*3))throw std::runtime_error("blank frame differs");
    rejects("not writable",[&]{p.owner(2,ram);});
    native[(8*2)*512*3]=1;
    rejects("not duplicated",[&]{p.compose(native,false);});
    p.reset();rejects("not sealed",[&]{p.compose(native,false);});
    p.begin();rejects("upload owner mismatch",[&]{p.upload(1);});
    rejects("invalid stream memory",[&]{p.memoryWrite({1,1,65536,1,0});});
    FramePipeline warm;std::vector<uint8_t> maps(0x272);
    warm.configureShip(maps);ram[0x79b]=0xf8;ram[0x79c]=0x91;
    auto frame=[&](uint64_t tag){warm.begin();warm.owner(tag,ram);warm.upload(tag);for(unsigned vline=0;vline<=224;vline++)warm.raster(vline,tag,v,c,r);};
    frame(1);
    if(warm.shipHistoryReady())throw std::runtime_error("first ship frame invented history");
    warm.seal();frame(2);
    if(!warm.shipHistoryReady())throw std::runtime_error("ship priming lost prior owner");
    warm.seal();warm.reset();frame(3);
    if(warm.shipHistoryReady())throw std::runtime_error("reset retained stale ship history");
    warm.seal();ram[0x79b]=0x9f;ram[0x79c]=0x9e;frame(4);warm.seal();
    ram[0x79b]=0xf8;ram[0x79c]=0x91;frame(5);
    if(warm.shipHistoryReady())throw std::runtime_error("other room used as ship history");
    warm.seal();
    warm.reset();ram[0x998]=0x12;frame(6);
    if(warm.shipHistoryReady())throw std::runtime_error("resume invented ship history");
    warm.seal();frame(7);if(!warm.shipHistoryReady())throw std::runtime_error("resume failed to prime ship history");warm.seal();
    std::cout<<"PASS: blank-frame repeatability and 15 intended frame/ownership/stream guards\n";
    std::cout<<"PASS: first-frame, next-frame, reset and cross-room ship history guards\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
