#include "raster_timeline.hpp"
#include <vector>
#include <iostream>
using namespace SMPWide;
void put(std::vector<uint8_t>&b,unsigned a,unsigned v){b[a]=v;b[a+1]=v>>8;}
void reg(std::vector<uint8_t>&b,Reg k,unsigned v){auto f=registerFields[unsigned(k)];for(unsigned i=0;i<f.size;i++)b[f.offset+i]=v>>(8*i);}
int main(){std::vector<uint8_t>ram(131072),rom(3145728),v(65536),pal(512),regs(205);put(ram,0x998,8);put(ram,0x79b,0xa98d);put(ram,0xfa8,0x40);put(ram,0xb04,288);put(ram,0xb06,112);for(unsigned i=0;i<128;i++)ram[0x371+i*4]=240;
ram[0x370]=24;ram[0x371]=104;ram[0x570]=1;put(ram,0x372,0x2800);for(unsigned y=0;y<8;y++)v[y*2]=255;put(pal,386,31);reg(regs,Reg::obj_priority2,8);
auto pixel=[&](int x){EnemySampler s(ram,rom,v,pal,regs);return s.pixel(x,105);};
auto p=pixel(280);if(!p || p->color!=31 || p->priority!=8)throw std::runtime_error("Samus margin OAM missing");
put(ram,0xb04,256);ram[0x370]=250;ram[0x570]=0;ram[0x570]=2; //16pixel sprite crossing native edge
if(pixel(255) || !pixel(256))throw std::runtime_error("Native center changed or crossing piece clipped");
for(auto bad:{std::pair{0x79b,0xa98e},std::pair{0xfa8,0},std::pair{0x372,0x2600},std::pair{0x372,0x2820}}){auto original=word(ram,bad.first);put(ram,bad.first,bad.second);if(pixel(256))throw std::runtime_error("Samus scope leaked");put(ram,bad.first,original);}
ram[0x371]=240;if(pixel(256))throw std::runtime_error("Hidden native Samus resurrected");
std::cout<<"PASS native Samus OAM margin, seam, hidden pose and room/palette/tile guards\n";}
