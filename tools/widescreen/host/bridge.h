// SPDX-License-Identifier: GPL-3.0-or-later
// C ABI keeps the qualified C++17 emulator separate from C++20 rendering code.
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t version;const uint8_t* data;size_t size;} SMPWideImage;
typedef struct {const uint8_t* data;size_t size;unsigned width,height;uint64_t clipped;const uint8_t* center_mask;size_t center_mask_size;int blur;} SMPWideOutput;
int smpwide_title_row(SMPWideImage vram,SMPWideImage palette,SMPWideImage registers,unsigned line,uint8_t* rgb);
int smpwide_cinematic_row(SMPWideImage vram,SMPWideImage palette,SMPWideImage oam,SMPWideImage registers,unsigned line,unsigned function,uint8_t* rgb);
int smpwide_cinematic_configure(void* context,const uint8_t* rom,size_t bytes);
int smpwide_cinematic_owner(void* context,SMPWideImage wram,int reset);
int smpwide_cinematic_stars(void* context,SMPWideImage wram);
int smpwide_cinematic_write(void* context,unsigned address,unsigned width,unsigned value);
int smpwide_cinematic_fetch(void* context,unsigned address);
int smpwide_cinematic_scene_row(void* context,SMPWideImage vram,SMPWideImage palette,SMPWideImage oam,SMPWideImage registers,unsigned line,unsigned function,unsigned scene,uint8_t* rgb);
void* smpwide_create(void);
void smpwide_destroy(void* context);
int smpwide_reset(void* context);
// Clear frame/effect/room history while retaining immutable ROM configuration.
int smpwide_reset_history(void* context);
// Track real room owners while Ceres Mode7 owns the visible raster.
int smpwide_track_scene(void* context,SMPWideImage wram);
int smpwide_configure_eye(void* context,const uint8_t* table,size_t bytes);
int smpwide_configure_ship(void* context,const uint8_t* maps,size_t bytes);
int smpwide_configure_enemies(void* context,const uint8_t* rom,size_t bytes);
int smpwide_ship_history_ready(void* context);
int smpwide_prime_frame(void* context);
int smpwide_powerbomb_fetch(void* context,unsigned address);
int smpwide_wram_write(void* context,unsigned address,unsigned width,unsigned value);
int smpwide_begin(void* context);
int smpwide_owner(void* context,SMPWideImage wram);
int smpwide_upload(void* context,uint64_t owner);
int smpwide_raster(void* context,unsigned line,uint64_t owner,SMPWideImage vram,SMPWideImage palette,SMPWideImage registers);
// Null VRAM/palette data explicitly reuses the previous immutable image.
// Size/version must match; registers remain a full image.
int smpwide_raster_retained(void* context,unsigned line,uint64_t owner,SMPWideImage vram,SMPWideImage palette,SMPWideImage registers);
int smpwide_memory_write(void* context,uint64_t cycle,unsigned space,unsigned address,unsigned width,unsigned value);
int smpwide_register_write(void* context,uint64_t cycle,unsigned line,unsigned h,unsigned address,SMPWideImage registers);
int smpwide_compose(void* context,const uint8_t* native_rgb,size_t bytes,int blur);
int smpwide_compose32(void* context,const uint32_t* native,unsigned pitch,unsigned width,unsigned height,int blur);
int smpwide_recompose(void* context,const uint8_t* native_rgb,size_t bytes,int blur);
// Output remains valid until compose/recompose/reset/destroy. Serialize all calls.
SMPWideOutput smpwide_output(void* context);
const char* smpwide_error(void* context);
#ifdef __cplusplus
}
#endif
