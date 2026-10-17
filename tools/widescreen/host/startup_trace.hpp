// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdio>
#include <cstdlib>

// Explicitly enabled host-startup diagnostics; never called from emulation.
inline void smpStartupTrace(const char* stage) {
  const char* path=std::getenv("SMP_GUI_STARTUP_LOG");
  if(!path || !*path)return;
  if(auto* file=std::fopen(path,"ab")) {
    std::fprintf(file,"%s\n",stage);
    std::fclose(file);
  }
}
