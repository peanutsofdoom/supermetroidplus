#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import argparse,hashlib
def main():
 p=argparse.ArgumentParser(description='Import a matching ROM without changing the source file.');p.add_argument('rom',type=Path);a=p.parse_args()
 data=a.rom.read_bytes()
 if len(data)==3146240:data=data[512:]
 expected='12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72'
 if hashlib.sha256(data).hexdigest()!=expected:p.error('Expected unmodified Super Metroid (Japan, USA), 3 MiB. Source file unchanged.')
 target=Path(__file__).resolve().parent.parent/'games/Super Metroid.sfc'
 if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest()!=expected:p.error('A different ROM is already installed; move it aside first.')
 target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);print('ROM ready. Launch through Steam with Proton on Deck.')
if __name__=='__main__':main()
