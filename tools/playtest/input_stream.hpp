// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace SMPPlaytest {
// Actual callback order, not host-frame input sampling. Poll values include
// latch-high reads; falling edges are separate timestamped records.
class InputStream {
  std::istream* input = nullptr;
  std::ostream* output = nullptr;
  uint64_t events = 0, latches = 0, polls = 0, bytes = 8;
  uint64_t lastCycle = 0, lastFrame = 0;
  bool ended = false;
  void (*failureHandler)(const std::string&)=nullptr;
  static constexpr uint64_t limit = 64ull * 1024 * 1024;
  [[noreturn]] void fail(const char* reason) const {
    auto message="input event " + std::to_string(events) + ": " + reason;
    if(failureHandler)failureHandler(message);
    throw std::runtime_error(message);
  }
  void put(uint64_t value, unsigned size) {
    char data[8];
    for(unsigned i=0;i<size;i++) data[i]=char(value>>(8*i));
    output->write(data,size);
    if(!*output) fail("write failed");
    bytes+=size;
  }
  uint64_t get(unsigned size) {
    unsigned char data[8];
    input->read(reinterpret_cast<char*>(data),size);
    if(!*input) fail("truncated stream");
    uint64_t value=0;
    for(unsigned i=0;i<size;i++) value|=uint64_t(data[i])<<(8*i);
    bytes+=size;
    return value;
  }
  int16_t event(unsigned tag,uint64_t cycle,uint64_t frame,unsigned port,
                unsigned device,unsigned index,int16_t value) {
    if(ended) fail("event after completion");
    if(port>1 || (tag==2 && index>=12)) fail("unsupported gamepad input");
    if(cycle<lastCycle || frame<lastFrame) fail("nonmonotonic callback");
    if(bytes+31+33>limit) fail("64 MiB input budget exceeded");
    if(output) {
      put(tag,1);put(cycle,8);put(frame,8);put(port,4);put(device,4);put(index,4);
      put(uint16_t(value),2);
    } else {
      if(get(1)!=tag) fail("callback kind differs");
      if(get(8)!=cycle) fail("guest cycle differs");
      if(get(8)!=frame) fail("native frame differs");
      if(get(4)!=port) fail("controller port differs");
      if(get(4)!=device) fail("controller device differs");
      if(get(4)!=index) fail("button query differs");
      uint16_t bits=uint16_t(get(2));std::memcpy(&value,&bits,sizeof(value));
      if(tag==1 && value) fail("invalid latch payload");
    }
    lastCycle=cycle;lastFrame=frame;events++;
    if(tag==1) latches++;else polls++;
    return value;
  }
public:
  // Core callbacks may run on private coroutine stacks. Their handler must
  // report/terminate without throwing through the platform unwinder.
  void onFailure(void (*handler)(const std::string&)){failureHandler=handler;}
  explicit InputStream(std::ostream& stream):output(&stream) {
    output->write("SMPI0001",8);
    if(!*output) fail("header write failed");
  }
  explicit InputStream(std::istream& stream):input(&stream) {
    char magic[8];input->read(magic,8);
    if(!*input || std::memcmp(magic,"SMPI0001",8)) fail("wrong stream identity");
  }
  void latch(uint64_t cycle,uint64_t frame,unsigned port) {
    event(1,cycle,frame,port,0,0,0);
  }
  int16_t poll(uint64_t cycle,uint64_t frame,unsigned port,unsigned device,
               unsigned index,int16_t observed) {
    return event(2,cycle,frame,port,device,index,observed);
  }
  void finish(uint64_t cycle,uint64_t frame) {
    if(ended || cycle<lastCycle || frame<lastFrame) fail("invalid completion");
    if(output) {
      put(255,1);put(cycle,8);put(frame,8);put(latches,8);put(polls,8);
      output->flush();if(!*output) fail("completion write failed");
    } else {
      if(get(1)!=255) fail("unconsumed input events");
      if(get(8)!=cycle || get(8)!=frame || get(8)!=latches || get(8)!=polls)
        fail("completion counters differ");
      if(input->peek()!=std::char_traits<char>::eof()) fail("trailing data");
    }
    ended=true;
  }
  uint64_t eventCount() const{return events;}
  uint64_t latchCount() const{return latches;}
  uint64_t pollCount() const{return polls;}
  uint64_t byteCount() const{return bytes;}
};
}
