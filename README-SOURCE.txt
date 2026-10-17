Super Metroid Plus 253/254 corresponding source snapshot

Contains the modified bsnes-hd GUI/core, renderer, launcher, report helpers,
recorded build commands, optimized PPU source and profile counts. No ROM/saves.
License notices are retained in the source tree; custom files use GPL-3.0-or-later.

To rebuild on Windows, install Python3 and a MinGW-w64 C++20-capable toolchain
with g++, windres and its standard libraries (the release used w64devkit).
Extract this source ZIP into a writable directory. Run:
  python rebuild_source.py --compiler C:/path/to/bin/g++.exe --version 254
Use --version 253 for the normal build. The script rebases the
recorded build paths and rebuilds the core, PPU, renderer and launcher locally.
Compiler/version/profile path differences can change binary hashes. This source
recipe is provided for rebuilding; friend package validation used the already
qualified release executables, not a fresh full-source rebuild.
