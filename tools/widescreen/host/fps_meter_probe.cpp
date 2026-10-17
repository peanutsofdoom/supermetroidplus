#include "fps_meter.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
  SmpFpsMeter m;m.sample(100,10);assert(m.fps()<0);
  for(unsigned i=1;i<=60;i++)m.sample(100+i,10+i/60.0);
  assert(std::abs(m.fps()-60)<0.001);
  m.sample(160,11.5);assert(std::abs(m.fps()-60)<0.001);
  for(unsigned i=1;i<=40;i++)m.sample(160+i,11+i/40.0);
  assert(std::abs(m.fps()-40)<0.001);
  m.sample(201,20);assert(m.fps()<0);
  m.sample(251,21);assert(std::abs(m.fps()-50)<0.001);
  m.sample(1,22);assert(m.fps()<0);
  m.sample(31,23);assert(std::abs(m.fps()-30)<0.001);
  m.reset();assert(m.fps()<0);
  std::cout<<"PASS wall-clock FPS, duplicate redraw exclusion, rate change, pause/reset recovery\n";
}
