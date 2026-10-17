#include "frame_pipeline.hpp"
#include <iostream>
#include <stdexcept>
using namespace SMPWide;
static void put(std::vector<uint8_t>& b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
static void reg(std::vector<uint8_t>& b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){try{
 auto check=[](bool v,const char* why){if(!v)throw std::runtime_error(why);};
 std::vector<uint8_t> ram(131072),rom(3145728),v(65536),p(512),r(205),eye(256);
 // A real ordering of Ceres owner states: Mode7 gameplay, outgoing fade,
 // destination setup, then Mode1 scrolling. No invented gameplay owner.
 put(ram,0x998,8);put(ram,0x79b,0xdf45);put(ram,0x7bb,0xdf57);put(ram,0x7a5,16);put(ram,0x7a7,48);put(ram,0x78d,0x8000);
 FramePipeline history;history.configureEye(eye);history.trackScene(1,ram);
 put(ram,0x998,11);put(ram,0x99c,0xe2db);history.trackScene(2,ram);
 put(ram,0x79b,0xdf8d);put(ram,0x7bb,0xdf9f);put(ram,0x7a5,32);put(ram,0x7a7,16);put(ram,0x99c,0xe38e);put(ram,0x791,0);put(ram,0x915,512);history.trackScene(3,ram);
 put(ram,0x99c,0xe4a9);put(ram,0x915,0);history.begin();history.owner(4,ram);history.upload(4);
 FramePipeline empty;empty.begin();bool rejected=false;try{empty.owner(4,ram);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"history-free door accidentally accepted");
 // Ceres's custom Ridley parts are present in native OAM at x>=256 even
 // though the generic enemy descriptors cannot describe all of them.
 ram.assign(131072,0);put(ram,0x998,8);put(ram,0x79b,0xe0b5);put(ram,0x7bb,0xe0c7);put(ram,0x79f,6);put(ram,0x7a5,32);put(ram,0x7a7,16);put(ram,0x7a9,2);put(ram,0xf78,0xe13f);put(ram,0x10002+(6*32+20)*2,0x8000);
 for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
 ram[0x370]=24;ram[0x371]=100;put(ram,0x372,0x3e01);ram[0x570]=1;
 for(unsigned y=0;y<8;y++){v[32+y*2]=255;v[96+y*2]=255;v[96+y*2+1]=255;}
 put(p,482,31);put(p,6,31744);reg(r,Reg::obj_priority3,8);
 auto object=[&](int x,bool fill=false){EnemySampler e(ram,rom,v,p,r);return e.pixel(x,101,fill);};
 auto q=object(280);check(q && q->color==31 && q->priority==8,"custom native margin piece missing");check(!object(239),"unqualified native-center fill");
 for(unsigned a:{0x998u,0x79bu,0x7bbu,0x79fu,0x7a5u}){auto n=word(ram,a);put(ram,a,0);check(!object(280),"native margin scene guard leaked");put(ram,a,n);}
 ram[0x371]=240;check(!object(280),"hidden native piece resurrected");ram[0x371]=100;
 // A high-priority old wall occludes a native sprite. The reference must
 // include the sprite, then reveal it when the wall's background is removed.
 ram[0x370]=240;ram[0x570]=0;reg(r,Reg::bgMode,1);reg(r,Reg::displayBrightness,15);reg(r,Reg::aboveEnable,1);reg(r,Reg::priority1,15);reg(r,Reg::screenAddress,0x5000);reg(r,Reg::obj_aboveEnable,1);
 put(v,(0x5000+12*32+30)*2,0x2003);
 auto owner=std::make_shared<const OwnerImage>(5,ram);SceneSelection scene{owner,owner,{},SceneKind::Ordinary};
 RowTimeline row(scene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));row.finish();
 check(row.centerCandidate(240),"opaque native piece blocked old-wall repair");
 check(row.pixel(240).rgb==std::array<uint8_t,3>{0,0,248},"native old-wall reference wrong");
 check(row.pixel(240,true).rgb==std::array<uint8_t,3>{248,0,0},"relocated wall still hides native sprite");
 // During the emergency quake the old wall starts at screen x239.
 reg(r,Reg::hoffset,1);
 RowTimeline shaken(scene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));shaken.finish();
 check(shaken.centerCandidate(239),"quake leaves leading old-wall seam");
 check(!shaken.centerCandidate(238),"quake repair reaches unrelated column");
 reg(r,Reg::hoffset,0);reg(r,Reg::bg2_hoffset,1);
 RowTimeline backgroundShake(scene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));backgroundShake.finish();
 check(backgroundShake.centerCandidate(239)&&!backgroundShake.centerCandidate(238),"independent BG2 quake seam escaped repair");
 check(backgroundShake.centerRange()==std::pair<int,int>{239,256},"wide arena repair range wrong");
 // Repeating backgrounds stop at Ceres's physical room boundaries, while
 // valid columns, unrelated rooms and death sequences retain their behavior.
 for(auto id:{std::pair{0xdf8du,0xdf9fu},std::pair{0xdfd7u,0xe003u},std::pair{0xe021u,0xe04du},std::pair{0xe06bu,0xe097u}}){
  put(ram,0x79b,id.first);put(ram,0x7bb,id.second);put(ram,0x7a5,16);
  for(unsigned game:{8u,9u,11u}){put(ram,0x998,game);put(ram,0x99c,0xe2db);SceneGeometry g(ram,ram,{},v,p,0,false,0,0);check(g.unusedPadding(-1,100)&&g.unusedPadding(256,100)&&!g.unusedPadding(0,100)&&!g.unusedPadding(255,100),"Ceres padding boundary wrong");}
  put(ram,0x998,21);SceneGeometry death(ram,ram,{},v,p,0,false,0,0);check(!death.unusedPadding(-1,100),"death flash clipped");
 }
 // Ceres's visible quake offset survives the final timer decrement.
 put(ram,0x998,8);put(ram,0x79b,0xe06b);put(ram,0x7bb,0xe097);put(ram,0x79f,6);put(ram,0x911,256);put(ram,0x183e,20);put(ram,0x1840,0);put(ram,0x943,0x8006);
 SceneGeometry tail(ram,ram,{},v,p,0,false,0,0);
 check(tail.quakeEdgeCandidate(0,255)&&!tail.quakeEdgeCandidate(1,255),"last Ceres quake frame lost its seam repair");
 check(tail.quakeEdgeCandidate(255,257)&&!tail.quakeEdgeCandidate(254,257),"opposite Ceres quake edge wrong");
 check(!tail.quakeEdgeCandidate(0,256),"unshifted Ceres frame repaired");
 put(ram,0x91d,0);reg(r,Reg::hoffset,255);
 auto quakeOwner=std::make_shared<const OwnerImage>(6,ram);SceneSelection quakeScene{quakeOwner,quakeOwner,{},SceneKind::Ordinary};
 RowTimeline quakeRow(quakeScene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));quakeRow.finish();
 check(quakeRow.centerRange()==std::pair<int,int>{0,1},"one-column quake scanned the whole native field");
 reg(r,Reg::hoffset,257);RowTimeline otherEdge(quakeScene,memoryImage(v,65536),memoryImage(p,512),r,101,{}, {},{},memoryImage(rom,3145728));otherEdge.finish();
 check(otherEdge.centerRange()==std::pair<int,int>{255,256},"right-edge quake range wrong");

 put(ram,0x7bb,0xe07d);SceneGeometry quiet(ram,ram,{},v,p,0,false,0,0);check(!quiet.quakeEdgeCandidate(0,255),"pre-escape quake scope leaked");
 // BG2-only floor clipping follows its own quake, not the static BG1 camera.
 put(ram,0x79b,0xe0b5);put(ram,0x7bb,0xe0c7);put(ram,0x7a5,32);put(ram,0x7a9,2);put(ram,0x911,0);put(ram,0x917,0);put(ram,0x921,256);put(ram,0x10002+(6*32+20)*2,0x8000);
 SceneGeometry floor(ram,ram,{},v,p,0,false,0,0);
 check(!floor.ceresFloorOutside(332,259)&&floor.ceresFloorOutside(333,259),"independent floor quake exposes right padding");
 check(!floor.ceresFloorOutside(335,256)&&floor.ceresFloorOutside(336,256),"stationary floor boundary moved");
 check(floor.ceresFloorOutside(0,255)&&!floor.ceresFloorOutside(1,255),"negative floor quake bounds wrong");
 put(ram,0x79b,0xe06b);SceneGeometry otherFloor(ram,ram,{},v,p,0,false,0,0);check(!otherFloor.ceresFloorOutside(336,259),"Ridley floor clipping leaked to hallway");
 std::cout<<"PASS Mode7/Mode1 scene history, native Ceres sprite margins, witnessed wall/OBJ recomposition and escape boundary guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
