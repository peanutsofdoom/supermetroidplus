struct PPU : Thread, PPUcounter {
  alwaysinline auto interlace() const -> bool { return display.interlace; }
  alwaysinline auto overscan() const -> bool { return display.overscan; }
  alwaysinline auto vdisp() const -> uint { return display.vdisp; }

  //ppu.cpp
  PPU();
  ~PPU();

  auto synchronizeCPU() -> void;
  static auto Enter() -> void;
  auto load() -> bool;
  auto power(bool reset) -> void;

  //main.cpp
  auto main() -> void;
  noinline auto cycleObjectEvaluate() -> void;
  template<uint Cycle> noinline auto cycleBackgroundFetch() -> void;
  noinline auto cycleBackgroundBegin() -> void;
  noinline auto cycleBackgroundBelow() -> void;
  noinline auto cycleBackgroundAbove() -> void;
  noinline auto cycleRenderPixel() -> void;
  template<uint> auto cycle() -> void;

  //io.cpp
  auto latchCounters(uint hcounter, uint vcounter) -> void;
  auto latchCounters() -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;
  #include <target-headless/presentation-registers.inc>
  auto diagnosticVideoSerialize(serializer& s) -> void {
    s.array(output);
    uint64 a = reinterpret_cast<uintptr_t>(screen.lineA);
    uint64 b = reinterpret_cast<uintptr_t>(screen.lineB);
    s.integer(a); s.integer(b);
    if(s.mode() == serializer::Load) {
      screen.lineA = reinterpret_cast<uint32*>(uintptr_t(a));
      screen.lineB = reinterpret_cast<uint32*>(uintptr_t(b));
    }
  }

  auto diagnosticMemory(unsigned space, uint8* target) -> bool {
    if(vram.mask != 0x7fff) return false;
    if(space == 1) for(unsigned n = 0; n < 65536; n++) target[n] = vram.data[n >> 1] >> ((n & 1) * 8);
    else if(space == 2) for(unsigned n = 0; n < 544; n++) target[n] = obj.oam.read(n);
    else if(space == 3) for(unsigned n = 0; n < 512; n++) target[n] = screen.cgram[n >> 1] >> ((n & 1) * 8);
    else return false;
    return true;
  }

private:
  //ppu.cpp
  alwaysinline auto step() -> void;
  alwaysinline auto step(uint clocks) -> void;

  //io.cpp
  alwaysinline auto addressVRAM() const -> uint16;
  alwaysinline auto readVRAM() -> uint16;
  alwaysinline auto writeVRAM(bool byte, uint8 data) -> void;
  alwaysinline auto readOAM(uint10 address) -> uint8;
  alwaysinline auto writeOAM(uint10 address, uint8 data) -> void;
  alwaysinline auto readCGRAM(bool byte, uint8 address) -> uint8;
  alwaysinline auto writeCGRAM(uint8 address, uint15 data) -> void;
  auto readIO(uint address, uint8 data) -> uint8;
  auto writeIO(uint address, uint8 data) -> void;
  auto updateVideoMode() -> void;

  struct VRAM {
    auto& operator[](uint address) { return data[address & mask]; }
    uint16 data[64 * 1024];
    uint16 mask = 0x7fff;
  } vram;

  uint32 output[512 * 480];
  uint32 lightTable[16][32768];

  struct {
    bool interlace;
    bool overscan;
    uint vdisp;
  } display;

  auto refresh() -> void;

  struct {
    uint4 version;
    uint8 mdr;
  } ppu1, ppu2;

  struct Latch {
    uint16 vram;
     uint8 oam;
     uint8 cgram;
     uint8 bgofsPPU1;
     uint3 bgofsPPU2;
     uint8 mode7;
     uint1 counters;
     uint1 hcounter;
     uint1 vcounter;

    uint10 oamAddress;
     uint8 cgramAddress;
  } latch;

  struct IO {
    //$2100  INIDISP
     uint1 displayDisable;
     uint4 displayBrightness;

    //$2102  OAMADDL
    //$2103  OAMADDH
    uint10 oamBaseAddress;
    uint10 oamAddress;
     uint1 oamPriority;

    //$2105  BGMODE
     uint1 bgPriority;
     uint8 bgMode;

    //$210d  BG1HOFS
    uint16 hoffsetMode7;

    //$210e  BG1VOFS
    uint16 voffsetMode7;

    //$2115  VMAIN
     uint1 vramIncrementMode;
     uint2 vramMapping;
     uint8 vramIncrementSize;

    //$2116  VMADDL
    //$2117  VMADDH
    uint16 vramAddress;

    //$211a  M7SEL
     uint2 repeatMode7;
     uint1 vflipMode7;
     uint1 hflipMode7;

    //$211b  M7A
    uint16 m7a;

    //$211c  M7B
    uint16 m7b;

    //$211d  M7C
    uint16 m7c;

    //$211e  M7D
    uint16 m7d;

    //$211f  M7X
    uint16 m7x;

    //$2120  M7Y
    uint16 m7y;

    //$2121  CGADD
     uint8 cgramAddress;
     uint1 cgramAddressLatch;

    //$2133  SETINI
     uint1 extbg;
     uint1 pseudoHires;
     uint1 overscan;
     uint1 interlace;

    //$213c  OPHCT
    uint16 hcounter;

    //$213d  OPVCT
    uint16 vcounter;
  } io;

  #include "mosaic.hpp"
  #include "background.hpp"
  #include "object.hpp"
  #include "window.hpp"
  #include "screen.hpp"

  Mosaic mosaic;
  Background bg1;
  Background bg2;
  Background bg3;
  Background bg4;
  Object obj;
  Window window;
  Screen screen;

  friend class PPU::Background;
  friend class PPU::Object;
  friend class PPU::Window;
  friend class PPU::Screen;
  friend class System;
  friend class PPUfast;
};

extern PPU ppu;
