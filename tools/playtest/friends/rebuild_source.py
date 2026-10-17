#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rebuild in a clean source snapshot with Python 3 and a MinGW-w64 toolchain."""
from pathlib import Path
import argparse,json,shlex,subprocess

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--compiler',type=Path,required=True);parser.add_argument('--version',type=int,choices=[253,254],default=254);a=parser.parse_args()
 root=Path(__file__).resolve().parent;gui=root/'development/bsnes-hd-gui/bsnes';cc=a.compiler.resolve();windres=cc.with_name('windres.exe')
 def rebase(value):return value.replace('F:\\SuperMetroidPlus',str(root)).replace('F:/SuperMetroidPlus',root.as_posix())
 def run(command):
  command=[rebase(x) for x in command]
  command[0]=str(windres if Path(command[0]).name.lower().startswith('windres') else cc)
  if '-o' in command:(gui/command[command.index('-o')+1]).parent.mkdir(parents=True,exist_ok=True)
  print(Path(command[0]).name,' '.join(command[1:]),flush=True);subprocess.run(command,cwd=gui,check=True)
 (gui/'obj').mkdir(exist_ok=True);(gui/'out').mkdir(exist_ok=True)
 for line in (root/'build/playthrough-make-plan.txt').read_text().splitlines():
  if line.startswith(('g++ ','gcc ','windres ')):run(shlex.split(line))
 for name in ['ppu-window.cpp','ppu-window-background.cpp','ppu-window-window.cpp']:
  f=root/'build/playtest/code-performance-20260928'/name;f.write_text(rebase(f.read_text()))
 # The supplied profile contains execution counts, not ROM data or saves.
 run(json.loads((root/'build/playtest/df8d-performance-20261005/ppu-trained.json').read_text())['command'])
 for command in json.loads((root/f'build/playtest/enemies-20260926-v{a.version}/build.json').read_text())['commands']:run(command)
 print('Built',root/f'build/playtest/enemies-20260926-v{a.version}/smp-enemies-v{a.version}.exe')

if __name__=='__main__':main()
