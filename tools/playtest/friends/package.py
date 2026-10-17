"""Seal a clean ROM-free portable friends package from the qualified release."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,zipfile
P=Path(__file__).resolve().parents[3];R=P/'build/playtest/friends-package-blue-scanner-20261013';S=P/'tools/playtest/friends'
name='Super Metroid Plus - Friends 253-254';out=R/name
sha=lambda b:hashlib.sha256(b).hexdigest()

def main():
 if out.exists():raise RuntimeError('Preserve earlier package staging; choose a new destination')
 out.mkdir(parents=True)
 proof=json.loads((P/'build/playtest/blue-scanner-20261013/final-validation.json').read_text())
 assert proof['builds']['coordinates']==254
 for version in [253,254]:
  exe=P/f'build/playtest/enemies-20260926-v{version}/smp-enemies-v{version}.exe'
  assert sha(exe.read_bytes())==proof['production']['versions'][str(version)]['executable_sha256']
  target=out/'app'/exe.name;target.parent.mkdir(exist_ok=True);shutil.copy2(exe,target)
 for dest,version in [('Play.exe',253),('Play With Coordinates.exe',254),('Configure Controls.exe',253)]:shutil.copy2(P/f'build/playtest/enemies-20260926-v{version}/Play.exe',out/dest)
 base=P/'preview/Owner Playable 2026-09-24'
 for folder in ['app/Database','app/Locale','app/Shaders']:
  if (base/folder).exists():shutil.copytree(base/folder,out/folder)
 for folder in ['games','saves','states','screenshots','patches','cheats','logs','reports','app/Firmware','tools','LICENSES','source']:(out/folder).mkdir(parents=True,exist_ok=True)
 shutil.copy2(base/'LICENSE-bsnes-hd.txt',out/'LICENSES/GPL-3.0.txt')
 shutil.copy2(P/'build/tools/w64devkit/COPYING.MinGW-w64-runtime.txt',out/'LICENSES/MinGW-w64-runtime.txt')
 settings=(base/'settings-template.bml').read_text()
 settings=re.sub(r'^  (Games|Saves|States|Screenshots|Patches|Cheats): .*$',lambda m:'  '+m[1]+': ../'+m[1].lower()+'/',settings,flags=re.M)
 buttons={'B':0,'Y':2,'A':1,'X':3,'L':6,'R':7,'Select':4,'Start':5}
 directions={'Up':'1/1/Lo','Down':'1/1/Hi','Left':'1/0/Lo','Right':'1/0/Hi'}
 for key in [*buttons,*directions]:
  pattern=r'^      '+key+r': (.*)$';m=re.search(pattern,settings,re.M);assert m
  suffix='3/'+str(buttons[key]) if key in buttons else directions[key]
  settings=re.sub(pattern,'      '+key+': '+m[1]+';0x045e028e/'+suffix,settings,flags=re.M)
 (out/'settings-template.bml').write_text(settings,encoding='utf-8')
 for f in S.glob('*.txt'):shutil.copy2(f,out/f.name)
 for f in ['Create-Report.ps1','Import-ROM.ps1','create_report.py','check_rom.py','Send-To-Deck.ps1','deck_setup.py','deck_bootstrap.py']:shutil.copy2(S/f,out/'tools'/f)
 shutil.copy2(P/'tools/playtest/install_deck.py',out/'tools/steam_setup.py')
 for name_,script in [('Create Report.cmd','Create-Report.ps1'),('Import ROM.cmd','Import-ROM.ps1'),('Send to Steam Deck.cmd','Send-To-Deck.ps1')]:
  (out/name_).write_text('@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\\'+script+'"\r\npause\r\n',encoding='ascii')
 (out/'Windows Capture Mode.cmd').write_text('@echo off\r\n>"%~dp0CAPTURE-SAFE-FULLSCREEN" echo smp-same-window-capture-v1\r\necho Windows capture mode enabled for this folder.\r\npause\r\n',encoding='ascii')
 (out/'games/PUT YOUR ROM HERE.txt').write_text('Put your matching unmodified ROM here as Super Metroid.sfc, or use Import ROM.cmd (Windows) / tools/check_rom.py (Deck).\n',encoding='utf-8')
 build={'package':'Super Metroid Plus friends playtest','package_revision':3,'date':'2026-10-13','normal_build':253,'coordinates_build':254,'platforms':['Windows x64','Steam Deck via Proton 10.0'],'rom_sha256':'12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72','rom_included':False,'personal_saves_included':False,'automatic_upload':False,'runtime_sha256':{f'app/smp-enemies-v{v}.exe':sha((out/f'app/smp-enemies-v{v}.exe').read_bytes()) for v in [253,254]},'validation':{'inherited_renderer_targeted_frames':5995,'inherited_renderer_probes':24,'inherited_hud_panel_probe_cycles':80,'inherited_hud_gui_cases':5,'scanner_replay_frames':320,'scanner_fallbacks_removed':135,'focused_renderer_probes':25,'blue_scanner_targeted_frames':1130,'blue_scanner_fallbacks_removed':724,'native_centers_checked':1670,'gui_comparison_frames_per_build':8472},'notes':'Qualified253/254 runtime with guided Windows-to-Deck LAN setup. Fresh friend package setup tested separately; hardware/controller differences remain part of testing.'}
 (out/'BUILD.json').write_text(json.dumps(build,indent=2),encoding='utf-8')
 source_files={}
 repo=P/'development/bsnes-hd-gui'
 tracked=subprocess.check_output(['git','-c','safe.directory='+repo.as_posix(),'-C',str(repo),'ls-files','-z','--cached','--others','--exclude-standard']).decode().split('\0')
 blocked={'.exe','.dll','.o','.a','.d','.sfc','.smc','.srm','.bst','.bsz','.mp4','.webm','.zip','.7z'}
 for rel in tracked:
  f=repo/rel
  if not rel or not f.is_file() or f.suffix.lower() in blocked or any(part in ['.git','obj','out'] for part in Path(rel).parts):continue
  source_files[f.relative_to(P).as_posix()]=f.read_bytes()
 for folder in ['tools/widescreen/host','tools/widescreen/native']:
  for f in (P/folder).glob('*'):
   if f.suffix in ['.cpp','.hpp','.h']:source_files[f.relative_to(P).as_posix()]=f.read_bytes()
 required=['tools/widescreen/tv_launcher.cpp','tools/playtest/recorder.cpp','tools/playtest/recorder.h','tools/playtest/build_playthrough.py','tools/playtest/build_optimized_ppu.py','tools/playtest/install_deck.py','build/playthrough-make-plan.txt']
 required += [f.relative_to(P).as_posix() for f in (P/'tools/playtest').glob('*.hpp')]
 required += [f'build/playtest/code-performance-20260928/{f}' for f in ['ppu-window.cpp','ppu-window-background.cpp','ppu-window-window.cpp']]
 required += [f'build/playtest/df8d-performance-20261005/{f}' for f in ['ppu-trained.json','ppu-trained.gcda']]
 required += [f'build/playtest/enemies-20260926-v{v}/build.json' for v in [253,254]]
 for rel in required:source_files[rel]=(P/rel).read_bytes()
 for f in S.glob('*'):
  if f.is_file():source_files[f.relative_to(P).as_posix()]=f.read_bytes()
 source_files['rebuild_source.py']=(S/'rebuild_source.py').read_bytes()
 source_files['README-SOURCE.txt']=b'''Super Metroid Plus 253/254 corresponding source snapshot\n\nContains the modified bsnes-hd GUI/core, renderer, launcher, report helpers,\nrecorded build commands, optimized PPU source and profile counts. No ROM/saves.\nLicense notices are retained in the source tree; custom files use GPL-3.0-or-later.\n\nTo rebuild on Windows, install Python3 and a MinGW-w64 C++20-capable toolchain\nwith g++, windres and its standard libraries (the release used w64devkit).\nExtract this source ZIP into a writable directory. Run:\n  python rebuild_source.py --compiler C:/path/to/bin/g++.exe --version 254\nUse --version 253 for the normal build. The script rebases the\nrecorded build paths and rebuilds the core, PPU, renderer and launcher locally.\nCompiler/version/profile path differences can change binary hashes. This source\nrecipe is provided for rebuilding; friend package validation used the already\nqualified release executables, not a fresh full-source rebuild.\n'''
 with zipfile.ZipFile(out/'source/Super-Metroid-Plus-253-254-source.zip','w',zipfile.ZIP_DEFLATED,9) as z:
  for rel,data in sorted(source_files.items()):z.writestr(rel,data)
 manifest={'files':{f.relative_to(out).as_posix():sha(f.read_bytes()) for f in sorted(out.rglob('*')) if f.is_file()}}
 (out/'PACKAGE-MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
 archive=R/'Super-Metroid-Plus-Friends-253-254.zip';assert not archive.exists()
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,9) as z:
  for f in sorted(out.rglob('*')):
   if f.is_file():z.write(f,Path(name)/f.relative_to(out))
   elif not any(f.iterdir()):z.writestr((Path(name)/f.relative_to(out)).as_posix()+'/',b'')
 summary={'archive':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive.read_bytes()),'payload_files':len(manifest['files']),'source_files':len(source_files),'rom_included':False,'saves_included':False}
 (R/'package-summary.json').write_text(json.dumps(summary,indent=2));print(summary)

if __name__=='__main__':main()
