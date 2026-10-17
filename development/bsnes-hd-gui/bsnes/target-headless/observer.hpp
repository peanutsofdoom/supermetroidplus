#pragma once
#include <cstdint>
namespace SMPTrace {
extern uint64_t cycles;
void bus(unsigned address, unsigned value, bool write);
void write(const char* space, unsigned address, unsigned value, unsigned width);
void latch(unsigned port);
void interrupt(const char* kind, const char* phase);
void dma(unsigned channel, const char* phase);
bool diagnosticDMAStall(unsigned channel);
void ownershipWrite(const char* space);
void raster();
void versionWrite(const char* space, unsigned address, unsigned value, unsigned width);
void versionBus(unsigned address, unsigned value, bool writing);
void compactInstruction(unsigned pc);
void compactSignal(const char* name, unsigned channel, unsigned a, unsigned b);
void instruction(unsigned pc);
void boundary(unsigned kind, unsigned channel = 0);
void signal(const char* name, unsigned channel, unsigned a, unsigned b);
}
