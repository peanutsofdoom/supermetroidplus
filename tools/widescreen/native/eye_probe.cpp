// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_state.hpp"
#include <fstream>
#include <iostream>
static std::vector<uint8_t> read(std::istream& in,size_t size){std::vector<uint8_t> v(size);in.read(reinterpret_cast<char*>(v.data()),size);if(!in)throw std::runtime_error("truncated eye fixture");return v;}
static unsigned number(std::istream& in){auto v=read(in,4);return unsigned(v[0])|unsigned(v[1])<<8|unsigned(v[2])<<16|unsigned(v[3])<<24;}
int main(int argc,char** argv){try{
  if(argc!=3)throw std::runtime_error("usage: eye-probe fixture output");
  std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary);
  if(!in||!out||number(in)!=0x31455945)throw std::runtime_error("eye fixture header");
  SMPWide::EyeState state;state.configure(read(in,256));uint64_t owners=0,writes=0,rows=0,active=0;
  while(in.peek()!=std::char_traits<char>::eof()){
    auto kind=number(in);
    if(kind==1){state.owner(read(in,131072));owners++;}
    else if(kind==2){auto address=number(in),value=number(in),width=number(in);state.write(address,value,width);writes++;}
    else if(kind==3){auto line=number(in);auto bytes=read(in,205);auto row=state.row(SMPWide::Registers(bytes),int(line));rows++;
      out.put(row?1:0);if(row){active++;out.put(row->beam?1:0);for(int x=-43;x<299;x++)out.put(row->inside(x)?1:0);}
    }else throw std::runtime_error("unknown eye event");
  }
  out.close();if(!out)throw std::runtime_error("eye output failed");
  std::cout<<"{\"owners\":"<<owners<<",\"writes\":"<<writes<<",\"rows\":"<<rows<<",\"window_rows\":"<<active<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
