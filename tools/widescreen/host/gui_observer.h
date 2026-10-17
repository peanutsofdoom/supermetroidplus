// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
void smpgui_request(bool requested);
void smpgui_tv(bool requested);
void smpgui_reduce_slowdown(bool requested);
bool smpgui_known();
void smpgui_display(unsigned viewportWidth,unsigned viewportHeight,unsigned outputWidth,unsigned outputHeight,bool wide);
void smpgui_load(const uint8_t* rom,size_t bytes);
void smpgui_reset();
bool smpgui_begin(bool compatible);
bool smpgui_active();
void smpgui_emulation_done();
void smpgui_present_begin();
void smpgui_present_done();
bool smpgui_report_due();
const uint32_t* smpgui_finish(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,bool compactTv=false);
void smpgui_report(const char* path);
const uint32_t* smpgui_coordinates(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,unsigned crop,bool wide);
void smpgui_clear_coordinates();

const uint32_t* smpgui_queue_surface(const uint32_t* data,unsigned pitch,unsigned width,unsigned height,unsigned crop,const char* code,unsigned seconds,bool complete,bool error);
