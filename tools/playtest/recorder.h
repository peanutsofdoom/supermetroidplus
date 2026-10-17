// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
std::string smpr_diagnostic_state();
void smpr_request(const char* directory,bool replay);
void smpr_request_compare(const char* directory,const char* originalExecutable);
void smpr_ready_ship(bool exploration=false);
bool smpr_ready_for_human();
uint64_t smpr_human_frames();
void smpr_limit(unsigned frames);
bool smpr_requested();
bool smpr_active();
bool smpr_replaying();
bool smpr_readonly_replay();
bool smpr_finished();
const char* smpr_status();
const uint8_t* smpr_initial_sram(size_t* size);
void smpr_before_power(const uint8_t* rom,size_t size,bool compatible);
void smpr_begin(const char* configuration,const char* frontend);
bool smpr_before_frame(const char* configuration,bool compatible);
void smpr_after_frame();
void smpr_tick();
void smpr_stop();
void smpr_abort(const char* reason);
void smpr_marker();
int16_t smpr_poll(unsigned port,unsigned device,unsigned input,int16_t observed);
void smpr_latch(unsigned port);
void smpr_bus(unsigned address,unsigned value,bool writing);
void smpr_write(const char* space,unsigned address,unsigned value,unsigned width);
void smpr_interrupt(const char* kind,const char* phase);
void smpr_dma(unsigned channel,const char* phase);
void smpr_signal(const char* name,unsigned channel,unsigned a,unsigned b);
void smpr_video(const uint32_t*,unsigned pitch,unsigned width,unsigned height,unsigned scale);
void smpr_audio(const double*,unsigned channels);
