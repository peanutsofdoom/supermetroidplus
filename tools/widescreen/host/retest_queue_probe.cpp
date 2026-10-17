#include "retest_queue.hpp"
#include "retest_queue_overlay.hpp"
#include <iostream>
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::filesystem::path root=argv[1];std::filesystem::create_directories(root/"checkpoints");
 for(auto code:{"B1","E1","T1"})std::ofstream(root/"checkpoints"/(std::string(code)+".bst"))<<'x';
 std::ofstream(root/"cases.tsv")<<"B1\tLEFT BACKGROUND\nE1\tPIPE BUGS\nT1\tUPPER ENTRY\n";
 SmpRetestQueue q;q.open(root/"cases.tsv");require(!q.tick(0,false),"Queue started inactive");require(q.tick(2,true),"Initial case missing");q.loaded(2);
 require(!q.tick(11.999,true) && q.remaining()==1,"Early transition");require(q.tick(12,true) && q.current().code=="E1","Ten-second advance");
 q.loaded(20);require(!q.tick(25,true),"Short case");require(!q.tick(26,false),"Pause advanced");require(!q.tick(200,false),"Paused duration counted");require(!q.tick(201,true),"Resume counted pause");require(!q.tick(205.99,true),"Pause shortened test");require(q.tick(206,true),"Resume did not advance");q.loaded(207);
 require(!q.tick(217,true) && q.phase()==SmpRetestQueue::Phase::Complete,"End of queue");require(!q.tick(1000,true),"Queue looped unexpectedly");
 for(auto line:{"B1\tONE\nB1\tTWO\n","../\tESCAPE\n","b1\tBAD\n","ZZ\tMISSING\n",""}){
  std::ofstream(root/"bad.tsv")<<line;bool rejected=false;try{SmpRetestQueue bad;bad.open(root/"bad.tsv");}catch(const std::exception&){rejected=true;}require(rejected,"Bad manifest accepted");
 }
 for(auto dimensions:{std::pair{2048u,448u},std::pair{684u,224u},std::pair{684u,480u}}){
  auto w=dimensions.first,h=dimensions.second,crop=h==480?16u:0u;std::vector<uint32_t> image(w*h,0x202030);smpDrawQueuePanel(image.data(),w,h,crop,"B1",10,false,false);
  unsigned changed=0,margin=w==2048?256:86;for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)if(image[y*w+x]!=0x202030){changed++;require(x<margin && y<crop+(h>=448?64:32),"Queue panel overlaps gameplay/HUD");}require(changed>100,"Queue overlay absent");
 }
 std::cout<<"PASS ten-second queue, pause timing, completion, manifest guards and HUD-margin overlay\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
