// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_pipeline.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
static std::vector<uint8_t> read(std::istream& in,size_t count) {
  std::vector<uint8_t> data(count);in.read(reinterpret_cast<char*>(data.data()),count);
  if(!in)throw std::runtime_error("truncated stream");
  return data;
}
static uint64_t number(std::istream& in,unsigned size=4) {
  uint8_t bytes[8];in.read(reinterpret_cast<char*>(bytes),size);
  if(!in)throw std::runtime_error("truncated stream number");
  uint64_t value=0;for(unsigned i=0;i<size;i++)value|=uint64_t(bytes[i])<<(8*i);
  return value;
}
int main(int argc,char** argv) {
  try {
    if(argc!=3)throw std::runtime_error("usage: stream-probe fixture output-directory");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("stream open failed");
    if(number(in)!=0x31464d53)throw std::runtime_error("stream format");
    auto expectedFrames=number(in),firstFrame=number(in);
    if(!expectedFrames || expectedFrames>100000)throw std::runtime_error("invalid stream frame count");
    SMPWide::FramePipeline pipeline;std::map<uint64_t,SMPWide::MemoryRef> images;
    uint64_t frames=0,owners=0,uploads=0,rasters=0,writes=0,registers=0,clipped=0,wramWrites=0;unsigned frame=0;bool eyeConfigured=false;
    while(in.peek()!=std::char_traits<char>::eof()) {
      auto op=number(in);
      if(op==0) {
        auto id=number(in,8),size=number(in);
        if(size!=205 && size!=512 && size!=65536 && size!=131072)throw std::runtime_error("image size");
        auto data=read(in,size);
        if(!images.emplace(id,SMPWide::memoryImage(data,size)).second)throw std::runtime_error("duplicate image id");
      } else if(op==1) {
        frame=unsigned(number(in));if(frame!=firstFrame+frames)throw std::runtime_error("frame sequence");pipeline.begin();
      }
      else if(op==2) {auto id=number(in,8);pipeline.owner(id,*images.at(id));owners++;}
      else if(op==3) {pipeline.upload(number(in,8));uploads++;}
      else if(op==4) {
        unsigned v=unsigned(number(in));auto owner=number(in,8),vr=number(in,8),cg=number(in,8),reg=number(in,8);
        pipeline.raster(v,owner,images.at(vr),images.at(cg),images.at(reg));rasters++;
      } else if(op==5) {
        auto cycle=number(in,8);unsigned space=unsigned(number(in)),a=unsigned(number(in)),width=unsigned(number(in)),value=unsigned(number(in));
        pipeline.memoryWrite({cycle,space,a,width,value});writes++;
      } else if(op==6) {
        auto cycle=number(in,8);unsigned v=unsigned(number(in)),h=unsigned(number(in)),a=unsigned(number(in));auto reg=number(in,8);
        pipeline.registerWrite(cycle,v,h,a,images.at(reg));registers++;
      } else if(op==7) {
        if(number(in)!=frame)throw std::runtime_error("frame end mismatch");
        bool blur=number(in);auto native=read(in,512*480*3);pipeline.seal();
        auto first=pipeline.compose(native,blur);
        for(unsigned repeat=0;repeat<2;repeat++) {
          auto again=pipeline.compose(native,blur);
          if(again.rgb!=first.rgb || again.clipped!=first.clipped)throw std::runtime_error("composition is not repeatable");
        }
        auto path=std::filesystem::path(argv[2])/(std::to_string(frame)+".rgb");
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(first.rgb.data()),first.rgb.size());
        out.close();if(!out)throw std::runtime_error("frame write failed");
        frames++;clipped+=first.clipped;
      } else if(op==8) {
        if(eyeConfigured)throw std::runtime_error("duplicate effect configuration");
        pipeline.configureEye(read(in,256));eyeConfigured=true;
      } else if(op==10) {
        pipeline.configureShip(read(in,0x272));
      } else if(op==9) {
        if(!eyeConfigured)throw std::runtime_error("WRAM event without effect configuration");
        auto address=number(in),value=number(in),width=number(in);
        pipeline.wramWrite(unsigned(address),unsigned(value),unsigned(width));wramWrites++;
      } else throw std::runtime_error("unknown stream event");
    }
    if(frames!=expectedFrames || !pipeline.complete())throw std::runtime_error("incomplete stream");
    std::cout<<"{\"frames\":"<<frames<<",\"owners\":"<<owners<<",\"uploads\":"<<uploads
      <<",\"rasters\":"<<rasters<<",\"writes\":"<<writes<<",\"registers\":"<<registers
      <<",\"clipped_margin_pixels\":"<<clipped;
    if(eyeConfigured)std::cout<<",\"wram_writes\":"<<wramWrites;
    std::cout<<"}\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 2;}
}
