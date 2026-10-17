// Explicitly resampled controller tape for the owner-approved gameplay variant.
// Not an exact replay reader; ordinary InputStream remains unchanged.
#pragma once
#include <array>
#include <vector>
#include <istream>
#include <stdexcept>
#include <cstdint>
namespace SMPPlaytest {
class FrameInput {
  std::vector<std::array<int16_t,12>> frames{21601};
public:
  explicit FrameInput(std::istream& in) {
    char magic[8];in.read(magic,8);if(std::string(magic,8)!="SMPI0001")throw std::runtime_error("input header");
    auto get=[&](unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++){int b=in.get();if(b<0)throw std::runtime_error("truncated input");v|=uint64_t(b)<<(8*i);}return v;};
    for(;;) {
      auto tag=get(1);if(tag==255){get(8);get(8);get(8);get(8);break;}
      get(8);auto frame=get(8),port=get(4);get(4);auto index=get(4),value=get(2);
      if((tag!=1 && tag!=2) || frame>=frames.size())throw std::runtime_error("input bounds");
      if(tag==2 && port==0){if(index>=12)throw std::runtime_error("button bounds");frames[frame][index]=int16_t(value);}
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("input trailer");
  }
  void latch(uint64_t,uint64_t,unsigned){}
  int16_t poll(uint64_t,uint64_t frame,unsigned port,unsigned,unsigned index,int16_t) {
    if(frame>=frames.size() || index>=12)throw std::runtime_error("poll bounds");
    return port?0:frames[frame][index];
  }
  void finish(uint64_t,uint64_t){}
};
}
