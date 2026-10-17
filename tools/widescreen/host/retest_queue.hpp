// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

struct SmpQueueCase {std::string code,label;std::filesystem::path checkpoint;};
class SmpRetestQueue {
public:
  enum class Phase {Off,Ready,Loading,Playing,Complete,Failed};
private:
  std::vector<SmpQueueCase> cases_;
  Phase phase_=Phase::Off;
  size_t index_=0;
  double elapsed_=0,last_=0;
  bool wasRunning_=false;
public:
  static constexpr double Duration=10.0;
  void open(const std::filesystem::path& manifest) {
    std::ifstream in(manifest);if(!in)throw std::runtime_error("Retest queue manifest missing");
    std::vector<SmpQueueCase> cases;std::set<std::string> codes;std::string line;
    while(std::getline(in,line)) {
      if(!line.empty() && line.back()=='\r')line.pop_back();
      if(line.empty() || line[0]=='#')continue;
      if(line.size()<4 || line.size()>100 || line[2]!='\t')throw std::runtime_error("Invalid queue case line");
      auto code=line.substr(0,2),label=line.substr(3);
      for(char c:code)if(!((c>='A' && c<='Z') || (c>='0' && c<='9')))throw std::runtime_error("Queue codes require two uppercase letters or digits");
      for(char c:label)if(c<32 || c>126)throw std::runtime_error("Invalid queue label");
      if(!codes.insert(code).second)throw std::runtime_error("Duplicate queue code");
      auto file=manifest.parent_path()/"checkpoints"/(code+".bst");
      if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Queue checkpoint missing");
      cases.push_back({code,label,file});if(cases.size()>64)throw std::runtime_error("Too many queue cases");
    }
    if(cases.empty())throw std::runtime_error("Retest queue is empty");
    cases_=std::move(cases);index_=0;elapsed_=0;last_=0;wasRunning_=false;phase_=Phase::Ready;
  }
  bool tick(double now,bool running) {
    if(phase_==Phase::Off || phase_==Phase::Failed || phase_==Phase::Complete)return false;
    if(!std::isfinite(now))throw std::runtime_error("Invalid queue clock");
    if(phase_==Phase::Playing && wasRunning_ && running)elapsed_+=std::max(0.0,now-last_);
    last_=now;wasRunning_=running;
    if(!running)return false;
    if(phase_==Phase::Playing && elapsed_>=Duration) {
      elapsed_=Duration;
      if(index_+1==cases_.size()){phase_=Phase::Complete;return false;}
      ++index_;phase_=Phase::Ready;
    }
    if(phase_==Phase::Ready){phase_=Phase::Loading;wasRunning_=false;return true;}
    return false;
  }
  void loaded(double now){if(phase_!=Phase::Loading)throw std::logic_error("Unexpected queue load");elapsed_=0;last_=now;wasRunning_=true;phase_=Phase::Playing;}
  void fail(){phase_=Phase::Failed;wasRunning_=false;}
  Phase phase() const{return phase_;}
  const SmpQueueCase& current() const {if(cases_.empty())throw std::logic_error("No queue case");return cases_[index_];}
  unsigned remaining() const {return phase_==Phase::Complete?0:unsigned(std::ceil(std::max(0.0,Duration-elapsed_)));}
  size_t index() const{return index_;}
  size_t count() const{return cases_.size();}
  bool hasLabel() const{return !cases_.empty() && phase_!=Phase::Off;}
};
