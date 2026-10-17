// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_pipeline.hpp"
#include <iostream>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 std::vector<uint8_t> ram(131072),v(65536),p(512),r(205),native(512*480*3);
 put(ram,0x998,8);put(ram,0x79b,0xdd58);put(ram,0x7bb,0xdd6e);put(ram,0x7a5,64);put(ram,0x7a7,16);put(ram,0x79f,5);put(ram,0xf78,0xec7f);put(ram,0xfb8,0xec3f);put(ram,0x7800,1);
 put(ram,0x10002+(6*64+15)*2,0x830f);put(ram,0x10002+(6*64+20)*2,0xff);
 reg(r,Reg::bgMode,1);reg(r,Reg::displayDisable,1);
 for(unsigned i=0;i<native.size();i+=3)native[i]=200;
 FramePipeline pipeline;unsigned tag=0;
 auto frame=[&](){pipeline.begin();pipeline.owner(++tag,ram);pipeline.upload(tag);for(unsigned y=0;y<=224;y++)pipeline.raster(y,tag,memoryImage(v,65536),memoryImage(p,512),memoryImage(r,205));pipeline.seal();auto out=pipeline.compose(native,false);for(unsigned x=0;x<342;x++)if(out.rgb[(100*342+x)*3])return x;throw std::runtime_error("native row disappeared");};
 if(frame()!=43)throw std::runtime_error("ordinary framing changed");
 put(ram,0x10002+(6*64+15)*2,0);put(ram,0x10002+(6*64+20)*2,0x830f);
 for(int offset=43;offset>=0;offset--)if(frame()!=unsigned(offset))throw std::runtime_error("arena pan snapped or skipped");
 if(frame()!=0)throw std::runtime_error("arena did not settle");pipeline.reset();if(frame()!=0)throw std::runtime_error("loaded expanded checkpoint replayed reveal");
 reg(r,Reg::displayDisable,0);reg(r,Reg::displayBrightness,15);
 auto owner=std::make_shared<const OwnerImage>(1,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,32);auto after=r;reg(after,Reg::screen_blendMode,1);row.registerWrite(1,784,0x2130,after);row.finish();
  if(*row.at(181).registers!=r || *row.at(182).registers!=after)throw std::runtime_error("late Mother Brain IRQ split moved");
 put(ram,0x10002+(6*64+15)*2,0x830f);put(ram,0x10002+(6*64+20)*2,0xff);
 owner=std::make_shared<const OwnerImage>(2,ram);scene={owner,owner,{},SceneKind::Ordinary};
 RowTimeline beforeExpansion(scene,memoryImage(v,65536),memoryImage(p,512),r,32);beforeExpansion.registerWrite(1,784,0x2130,after);beforeExpansion.finish();
 put(ram,0x7800,4);owner=std::make_shared<const OwnerImage>(2,ram);scene={owner,owner,{},SceneKind::Ordinary};bool rejected=false;
 try{RowTimeline invalid(scene,memoryImage(v,65536),memoryImage(p,512),r,32);invalid.registerWrite(1,784,0x2130,after);}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("late colour reset escaped intro scope");
 std::cout<<"PASS continuous43-frame Mother Brain reveal, checkpoint framing and native late-IRQ split\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
