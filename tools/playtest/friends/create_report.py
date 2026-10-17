#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a local, deliberately small report ZIP. No uploads or save/ROM copies."""
from pathlib import Path
import argparse,datetime,json,platform,uuid,zipfile

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--no-open',action='store_true');parser.parse_args()
 root=Path(__file__).resolve().parent.parent
 name='SMP-254-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6]
 dest=root/'reports'/name;dest.mkdir(parents=True)
 included=[];skipped=[]
 for rel in ['BUILD.json','REPORT TEMPLATE.txt','logs/last-session.json','logs/launch-plan.json','logs/launcher-session.json']:
  source=root/rel
  if not source.is_file():continue
  if source.is_symlink() or source.stat().st_size>2*1024*1024:skipped.append(rel);continue
  try:
   text=source.read_text(encoding='utf-8-sig')
   for path in [str(root),root.as_posix(),str(Path.home()),Path.home().as_posix()]:
    for value in [path,path.replace('\\','\\\\')]:text=text.replace(value,'<local-folder>')
   target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text,encoding='utf-8');included.append(rel)
  except (OSError,UnicodeError):skipped.append(rel)
 data={'build':'253 normal / 254 Coordinates','os':platform.system(),'os_release':platform.release(),'architecture':platform.machine(),'included':included,'skipped':skipped,'automatic_upload':False,'saves_included':False,'rom_included':False}
 (dest/'report-details.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
 archive=dest.with_suffix('.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(dest.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(dest.parent))
 print(f'Created: {archive}\nAttach this ZIP and a short clip or screenshot to your Discord issue thread. Paste the report template into the message.')

if __name__=='__main__':main()
