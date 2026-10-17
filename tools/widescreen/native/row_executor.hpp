// SPDX-License-Identifier: GPL-3.0-or-later
// Persistent workers for independent rows of one immutable frame. run() is
// synchronous and calls on one executor must be serialized by its owner.
#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
namespace SMPWide {
class RowExecutor {
  unsigned count_,finished=0;
  uint64_t generation=0;
  bool stopping=false;
  std::mutex mutex;
  std::condition_variable start,complete;
  std::function<void(unsigned)> task;
  std::vector<std::exception_ptr> errors;
  std::vector<std::thread> workers;
  void work(unsigned index) {
    uint64_t seen=0;
    for(;;) {
      std::unique_lock lock(mutex);
      start.wait(lock,[&]{return stopping || generation!=seen;});
      if(stopping)return;
      seen=generation;lock.unlock();
      try {task(index);} catch(...) {errors[index]=std::current_exception();}
      lock.lock();++finished;complete.notify_one();
    }
  }
public:
  explicit RowExecutor(unsigned count=std::min(4u,std::max(1u,std::thread::hardware_concurrency()))):count_(count),errors(count) {
    if(!count || count>4)throw std::invalid_argument("row worker count");
    try {for(unsigned i=1;i<count_;i++)workers.emplace_back([this,i]{work(i);});}
    catch(...) {{std::lock_guard lock(mutex);stopping=true;}start.notify_all();for(auto& t:workers)t.join();throw;}
  }
  ~RowExecutor() {{std::lock_guard lock(mutex);stopping=true;}start.notify_all();for(auto& t:workers)t.join();}
  unsigned count() const {return count_;}
  void run(std::function<void(unsigned)> action) {
    {std::lock_guard lock(mutex);task=std::move(action);std::fill(errors.begin(),errors.end(),nullptr);finished=0;++generation;}
    start.notify_all();
    try {task(0);} catch(...) {errors[0]=std::current_exception();}
    {std::unique_lock lock(mutex);complete.wait(lock,[&]{return finished==count_-1;});task={};}
    for(auto& error:errors)if(error)std::rethrow_exception(error);
  }
};
}
