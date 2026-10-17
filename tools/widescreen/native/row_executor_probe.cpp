// SPDX-License-Identifier: GPL-3.0-or-later
#include "row_executor.hpp"
#include <array>
#include <iostream>
int main() {
  for(unsigned count=1;count<=4;count++) {
    SMPWide::RowExecutor rows(count);
    std::array<unsigned,4> totals{};
    for(unsigned pass=0;pass<1000;pass++)rows.run([&](unsigned i){totals[i]++;});
    for(unsigned i=0;i<count;i++)if(totals[i]!=1000)throw std::runtime_error("lost row job");
    bool caught=false;
    try {rows.run([](unsigned i){throw std::runtime_error(std::to_string(i));});}
    catch(const std::runtime_error& e){caught=std::string(e.what())=="0";}
    if(!caught)throw std::runtime_error("exception order");
    rows.run([&](unsigned i){totals[i]++;});
    for(unsigned i=0;i<count;i++)if(totals[i]!=1001)throw std::runtime_error("recovery");
  }
  std::cout<<"PASS persistent row jobs, exception ordering, recovery and shutdown\n";
}
