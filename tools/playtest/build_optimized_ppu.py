"""Build a separate, behavior-preserving PPU candidate; retain the pinned core."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--window-gates', action='store_true')
    parser.add_argument('--tune', choices=('generic', 'znver2'), default='generic')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise ValueError('Preserve the previous PPU candidate')
    output.parent.mkdir(parents=True, exist_ok=True)
    gui = ROOT / 'development/bsnes-hd-gui/bsnes'
    src = gui / 'sfc/ppu'
    background = (src / 'background.cpp').read_text()
    old = '''  uint8 color = 0;
  if(io.mode >= Mode::BPP2) color |= (tile.data[0] & 3) << 0; tile.data[0] >>= 2;
  if(io.mode >= Mode::BPP4) color |= (tile.data[1] & 3) << 2; tile.data[1] >>= 2;
  if(io.mode >= Mode::BPP8) color |= (tile.data[2] & 3) << 4; tile.data[2] >>= 2;
  if(io.mode >= Mode::BPP8) color |= (tile.data[3] & 3) << 6; tile.data[3] >>= 2;'''
    new = '''  uint8 color = 0;
  if(io.mode >= Mode::BPP2) color |= (tile.data[0] & 3) << 0;
  if(io.mode >= Mode::BPP4) color |= (tile.data[1] & 3) << 2;
  if(io.mode >= Mode::BPP8) color |= (tile.data[2] & 3) << 4;
  if(io.mode >= Mode::BPP8) color |= (tile.data[3] & 3) << 6;
  // Preserve all four independent shift registers, including inactive planes.
  static_assert(sizeof(tile.data)==sizeof(uint64_t));
  static_assert(std::is_trivially_copyable<decltype(tile.data)>::value);
  uint64_t planes;
  ::memcpy(&planes,tile.data,sizeof planes);
  planes=(planes>>2)&UINT64_C(0x3fff3fff3fff3fff);
  ::memcpy(tile.data,&planes,sizeof planes);'''
    if background.count(old) != 1:
        raise ValueError('Review changed background implementation before optimizing')
    background = background.replace(old, new)
    background = background.replace('#include "mode7.cpp"', '#include "' + (src / 'mode7.cpp').as_posix() + '"')
    bg = output.with_name(output.stem + '-background.cpp')
    bg.write_text(background)
    replacements = {'background.cpp': bg}
    if args.window_gates:
        text = (src / 'window.cpp').read_text()
        for layer in ('bg1', 'bg2', 'bg3', 'bg4', 'obj'):
            old = f'if(test(io.{layer}.oneEnable,'
            if text.count(old) != 1:
                raise ValueError('Review changed window implementation')
            text = text.replace(old, f'if((io.{layer}.aboveEnable || io.{layer}.belowEnable) && test(io.{layer}.oneEnable,')
        window = output.with_name(output.stem + '-window.cpp')
        window.write_text(text)
        replacements['window.cpp'] = window
    entry = output.with_suffix('.cpp')
    text = (src / 'ppu.cpp').read_text()
    text = re.sub(r'#include "([^"]+)"', lambda m: '#include "' + replacements.get(m[1], src / m[1]).as_posix() + '"', text)
    entry.write_text(text)
    line = next(l for l in (ROOT / 'build/playthrough-make-plan.txt').read_text().splitlines() if '-c sfc/ppu/ppu.cpp' in l)
    command = shlex.split(line)
    command[0] = str(ROOT / 'build/tools/w64devkit/bin/g++.exe')
    command.remove('-MMD')
    command.remove('-MP')
    i = command.index('-MF')
    del command[i:i + 2]
    command[command.index('sfc/ppu/ppu.cpp')] = str(entry)
    command[command.index('obj/sfc-ppu.o')] = str(output)
    command.insert(1, '-mtune=' + args.tune)
    result = subprocess.run(command, cwd=gui, capture_output=True, text=True)
    output.with_suffix('.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stderr[-2000:])
    files = [output, entry, *replacements.values(), *sorted(p for p in src.rglob('*') if p.suffix in ('.cpp', '.hpp'))]
    output.with_suffix('.json').write_text(json.dumps(dict(command=command, window_gates=args.window_gates, tune=args.tune,
        sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}), indent=2))
    print(output)


if __name__ == '__main__':
    main()
