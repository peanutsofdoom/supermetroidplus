// SPDX-License-Identifier: GPL-3.0-or-later
// This caller deliberately compiles as C++17 and includes no renderer headers.
#include "bridge.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv) {
  void* context=nullptr;
  try {
    require(argc==2,"usage: bridge-probe registers");
    std::vector<uint8_t> reg(205),ram(131072),vram(65536),palette(512),native(512*480*3);
    std::ifstream in(argv[1],std::ios::binary);in.read(reinterpret_cast<char*>(reg.data()),reg.size());require(bool(in),"register input");
    ram[0x998]=8;ram[0x7a5]=16;ram[0x7a7]=16;
    context=smpwide_create();require(context,"create");
    SMPWideImage owner{1,ram.data(),ram.size()},v{2,vram.data(),vram.size()},c{3,palette.data(),palette.size()},r{4,reg.data(),reg.size()};
    require(!smpwide_owner(context,owner),"owner outside frame accepted");
    require(std::string(smpwide_error(context)).find("not writable")!=std::string::npos,"wrong owner guard");
    require(!smpwide_recompose(context,native.data(),native.size(),0),"unsealed recompose accepted");
    std::vector<uint8_t> eyeTable(256);
    require(!smpwide_configure_eye(context,nullptr,256),"null eye table accepted");
    require(!smpwide_configure_eye(context,eyeTable.data(),255),"short eye table accepted");
    require(smpwide_configure_eye(context,eyeTable.data(),256),"eye setup rejected");
    require(!smpwide_wram_write(context,0,1,0),"WRAM write outside frame accepted");
    require(smpwide_reset(context) && smpwide_begin(context) && smpwide_owner(context,owner) && smpwide_upload(context,1),"begin/owner/upload");
    ram[0x7a5]=0; // The retained owner must be a copy, not this mutable buffer.
    require(smpwide_raster(context,0,1,v,c,r),"first raster");
    SMPWideImage retainedV{v.version,nullptr,v.size},retainedC{c.version,nullptr,c.size};
    auto wrongVersion=retainedV;wrongVersion.version++;
    require(!smpwide_raster_retained(context,1,1,wrongVersion,retainedC,r),"unknown retained version accepted");
    auto wrongSize=retainedC;wrongSize.size--;
    require(!smpwide_raster_retained(context,1,1,retainedV,wrongSize,r),"wrong retained size accepted");
    vram[0]=1;
    require(!smpwide_raster(context,1,1,v,c,r),"reused version accepted changed bytes");
    require(std::string(smpwide_error(context)).find("without version")!=std::string::npos,"wrong version guard");
    vram[0]=0;
    for(unsigned line=1;line<=224;line++)require(smpwide_raster_retained(context,line,1,retainedV,retainedC,r),"retained raster");
    require(smpwide_compose(context,native.data(),native.size(),0),smpwide_error(context));
    auto output=smpwide_output(context);
    require(output.width==342 && output.height==224 && output.size==342*224*3,"output shape");
    for(size_t i=0;i<output.size;i++)require(output.data[i]==0,"blank output changed");
    require(smpwide_recompose(context,native.data(),native.size(),0),"sealed recompose rejected");
    output=smpwide_output(context);
    for(size_t i=0;i<output.size;i++)require(output.data[i]==0,"recompose changed output");
    require(smpwide_reset_history(context) && smpwide_output(context).size==0,"history reset kept output");
    require(!smpwide_upload(context,1),"history reset retained upload");
    require(!smpwide_raster_retained(context,0,1,retainedV,retainedC,r),"history reset retained raster cache");
    require(smpwide_reset(context) && smpwide_output(context).size==0,"reset kept output");
    require(!smpwide_upload(context,1),"reset retained active frame");
    require(!smpwide_begin(nullptr),"null context accepted");
    require(smpwide_configure_eye(context,eyeTable.data(),256) && smpwide_begin(context),"effect reset setup failed");
    ram[0x7a5]=16;
    require(smpwide_owner(context,owner),"effect owner rejected");
    require(!smpwide_configure_eye(context,eyeTable.data(),256),"table change during frame accepted");
    require(!smpwide_wram_write(context,131071,2,0),"out of bounds WRAM write accepted");
    require(smpwide_wram_write(context,0x200,1,7),"committed WRAM rejected");
    owner.version=2;
    require(!smpwide_owner(context,owner),"stale effect owner accepted");
    ram[0x200]=7;
    require(smpwide_owner(context,owner),"matching effect owner rejected");
    smpwide_destroy(context);context=nullptr;
    std::cout<<"PASS: C++17 caller / C++20 renderer ABI, owned images, version guard and reset\n";
  }catch(const std::exception& e){smpwide_destroy(context);std::cerr<<e.what()<<"\n";return 2;}
}
